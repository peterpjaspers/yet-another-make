#include "Patch.h"
#include "MonitorLogging.h"

#include <windows.h>
#include <imagehlp.h>

#include <set>
#include <map>
#include <vector>

using namespace std;

namespace AccessMonitor {

    namespace {

        struct Patch {
            const string* library;
            PatchFunction original;
            PatchFunction* address;
            Patch() : library( nullptr ), original( nullptr ), address( nullptr ) {}
        };
        map< string, HMODULE > patchedLibraries;
        map< string, PatchFunction > registeredPatches;
        map< PatchFunction, Patch > functionToPatch;
        bool librariesPatched = false;

    }

    string toUpper( const string& name ) {
        string upname;
        for (auto c = name.begin(); c != name.end(); ++c ) upname += toupper( *c );
        return upname;
    }

    // Patch a single entry of an import address table (IAT).
    // 1 - Enable write access to (virtual) memory location of entry
    // 2 - Write patch value (address of function)
    // 3 - Restore access mode to (virtual) memory location of entry
    void patchImportEntry( PatchFunction* address, PatchFunction function ) {
        DWORD protection = 0;
        VirtualProtect((void*)address, sizeof( PatchFunction ), PAGE_READWRITE, &protection );
        *address = function;
        VirtualProtect((void*)address, sizeof( PatchFunction ), protection, nullptr );
    }

    // Registered functions may not actually be patched (imported) in an executable or DLL.
    // In that case, repatchFunction and unpatchFunction do nothing.
    bool repatchFunction( const PatchFunction function ) {
        if (0 < functionToPatch.count( function )) {
            auto patchData = functionToPatch[ function ];
            patchImportEntry( patchData.address, function );
            return true;
        }
        return false;
    }
    bool unpatchFunction( const PatchFunction function ) {
        if (0 < functionToPatch.count( function )) {
            auto patchData = functionToPatch[ function ];
            if (patchData.original != nullptr) {
                // Restore to original only if function was not re-patched by some other party
                if (function == *patchData.address) {
                    patchImportEntry( patchData.address, patchData.original );
                    return true;
                } else {
                    if (debugLog( PatchedFunction )) { debugRecord() << L"      Function in " << widen( *patchData.library ) << L" was repatched!" << record; }
                }
            }
        }
        return false;
    }
    void patchAll() {
        for ( auto patchFunction : registeredPatches ) {
            if (repatchFunction( patchFunction.second )) {
                auto patchData = functionToPatch[ patchFunction.second ];
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Patched function " << widen( patchFunction.first ) << " in " << widen( *patchData.library ) << record; }
            }
        }
    }
    void unpatchAll() {
        for ( auto patchFunction : registeredPatches ) {
            if (unpatchFunction( patchFunction.second )) {
                auto patchData = functionToPatch[ patchFunction.second ];
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Unpatched function " << widen( patchFunction.first ) << " in " << widen( *patchData.library ) << record; }
            }
        }        
    }

    // Parse Import Address Table (IAT) of named libray.
    // Parses program executable if name is empty string.
    // May only be called when no patches are in effect.
    void parseLibrary( const string& libName ) {
        static const char* signature = "void parseLibrary( const string& libName )";
        if (librariesPatched) throw string( signature ) + string( " - Parsing library IATs while libraries are patched!" );
        auto libraryName = toUpper( libName );
        if (patchedLibraries.count( libraryName ) == 0)  {
            if (debugLog( ParseLibrary )) { debugRecord() << L"Parsing " << ((0 < libraryName.size()) ? widen( libraryName.c_str() ) : L"program executable" )<< record; }
            HMODULE library = LoadLibraryExA( libraryName.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
            patchedLibraries[ libraryName ] = library;
            static uint64_t Ordinal = 0x8000000000000000;
            static uint64_t Last = 0xFFFF;
            HMODULE imageBase = GetModuleHandleA( (0 < libraryName.size()) ? libraryName.c_str() : nullptr );
            IMAGE_NT_HEADERS* imageHeaders = (IMAGE_NT_HEADERS*)((char*)imageBase + ((IMAGE_DOS_HEADER*)imageBase)->e_lfanew);
            IMAGE_DATA_DIRECTORY importsDirectory = imageHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ];
            IMAGE_IMPORT_DESCRIPTOR* importDescriptor = (IMAGE_IMPORT_DESCRIPTOR*)(importsDirectory.VirtualAddress + (char*)imageBase);
            while ((importDescriptor->Name != 0) && ((importDescriptor->Name & Ordinal) == 0) && (importDescriptor->Name != Last)) {
                string importLibName( (const char*)(importDescriptor->Name + (char*)imageBase) );
                string importLibraryName = toUpper( importLibName );
                HMODULE importLibrary = LoadLibraryExA( importLibraryName.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
                patchedLibraries[ importLibraryName ] = importLibrary;
                if (debugLog( ParseLibrary )) { debugRecord() << L"Parsing " << widen( importLibraryName.c_str() ) << L" IAT" << record; }
                IMAGE_THUNK_DATA* originalFirstThunk = (IMAGE_THUNK_DATA*)((char*)imageBase + importDescriptor->OriginalFirstThunk);
                IMAGE_THUNK_DATA* firstThunk = (IMAGE_THUNK_DATA*)((char*)imageBase + importDescriptor->FirstThunk);
                while ((originalFirstThunk->u1.AddressOfData != 0) && ((originalFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) == 0)) {
                    IMAGE_IMPORT_BY_NAME* importName = (IMAGE_IMPORT_BY_NAME*)((char*)imageBase + originalFirstThunk->u1.AddressOfData);
                    string name( (char *)importName->Name );
                    if (debugLog( ImportedFunction )) { debugRecord() << L"    Imported function " << widen( name ) << record; }
                    DWORD protection = 0;
                    if (0 < registeredPatches.count( name )) {
                        PatchFunction function = registeredPatches[ name ];
                        Patch& patchData = functionToPatch[ function ];
                        if (debugLog( PatchedFunction )) { debugRecord() << L"      Located IAT patch function " << widen( name ) << " in " << widen( importLibraryName ) << record; }
                        patchData.library = &(patchedLibraries.find( importLibraryName )->first);
                        patchData.address = reinterpret_cast<PatchFunction*>(&firstThunk->u1.Function);
                        patchData.original = *patchData.address;
                    }
                    ++originalFirstThunk;
                    ++firstThunk;
                }
                importDescriptor++;
            }
        }
    } // void parseLibrary( const string& libName )
    void parseLibrary( const wstring& libName, bool force ) { parseLibrary( narrow( libName ) ); }

    void registerPatch( std::string name, PatchFunction function ) {
        static const char* signature = "void registerPatch( std::string name, PatchFunction function, std::string library )";
        if (0 < registeredPatches.count( name )) throw string( signature ) + string( " - Function " ) + name + string( " already registered!" );
        registeredPatches[ name ] = function;
        if (debugLog( RegisteredFunctions )) debugRecord() << L"Registered function " << widen( name ) << record;;

    }
    void unregisterPatch( std::string name ) {
        static const char* signature = "void unregisterPatch( std::string name )";
        if (registeredPatches.count( name ) == 0) throw string( signature ) + string( " - Function " ) + name + string( " not registred!" );
        registeredPatches.erase( name );
        if (debugLog( RegisteredFunctions )) debugRecord() << L"Unregistered function " << widen( name ) << record;;
    }

    PatchFunction original( std::string name ) { return functionToPatch[ registeredPatches[ name ] ].original; }
    PatchFunction original( PatchFunction function ) { return functionToPatch[ function ].original; }
    PatchFunction patched( std::string name ) { return registeredPatches[ name ]; }
    const std::string& patchedLibrary( std::string name ) { return *functionToPatch[ registeredPatches[ name ] ].library; }
    const std::string& patchedLibrary( PatchFunction function ){ return *functionToPatch[ function ].library; }

    bool pathOverridden( std::string name ) {
        PatchFunction function = patched( name );
        Patch& patch = functionToPatch[ function ];
        return( (*patch.address != patch.original) && (*patch.address != function) );
    }

    void patch() {
        static const char* signature = "void patch()";
        if (librariesPatched) throw string( signature ) + string( " - Libraries already patched!" );
        if (debugLog( ParseLibrary )) debugRecord() << L"Parsing libraries..." << record;
        parseLibrary( "" );
        if (debugLog( ParseLibrary )) debugRecord() << L"Done Parsing libraries..." << record;
        patchAll();
        librariesPatched = true;
    }
    void unpatch() {
        static const char* signature = "void unpatch()";
        if (!librariesPatched) throw string( signature ) + string( " - Libraries not patched!" );
        unpatchAll();
        for ( auto lib : patchedLibraries ) FreeLibrary( lib.second );
        patchedLibraries.clear();
        functionToPatch.clear();
        librariesPatched = false;
    }

} // namespace AccessMonitor

