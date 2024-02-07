#include "Patch.h"
#include "MonitorProcess.h"
#include "Log.h"

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
    // 3 - Restore access writes to (virtual) memory location of entry
    void patchImportEntry( PatchFunction* address, PatchFunction function ) {
        DWORD protection = 0;
        VirtualProtect((void*)address, sizeof( PatchFunction ), PAGE_READWRITE, &protection );
        *address = function;
        VirtualProtect((void*)address, sizeof( PatchFunction ), protection, nullptr );
    }

    // Registered functions may not actually be patched in an executable or DLL.
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
                    patchData.original = nullptr;
                    return true;
                } else {
                    if (monitorLog( PatchedFunction )) { monitorLog() << L"      Function in " << widen( *patchData.library ) << L" was repatched!" << record; }
                }
            }
        }
        return false;
    }
    void patchAll() {
        for ( auto patchFunction : registeredPatches ) {
            if (repatchFunction( patchFunction.second )) {
                auto patchData = functionToPatch[ patchFunction.second ];
                if (monitorLog( PatchedFunction )) { monitorLog() << L"      Patched function " << widen( patchFunction.first ) << " in " << widen( *patchData.library ) << record; }
            }
        }
    }
    void unpatchAll() {
        for ( auto patchFunction : registeredPatches ) {
            if (unpatchFunction( patchFunction.second )) {
                auto patchData = functionToPatch[ patchFunction.second ];
                if (monitorLog( PatchedFunction )) { monitorLog() << L"      Unpatched function " << widen( patchFunction.first ) << " in " << widen( *patchData.library ) << record; }
            }
        }        
    }

    typedef HMODULE(*TypeLoadLibraryExA)(LPCSTR,HANDLE,DWORD);
/*
    bool validateNopSignature( PatchFunction function ) {
        static const uint8_t signature[ 7 ] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x8b, 0xff };
        uint8_t* patch = const_cast<uint8_t*>(&signature[ 0 ]);
        uint8_t* address = (uint8_t*)function - 5;
        bool validated = true;
        for (int i = 0; i < 7; ++i) {
            if (*patch++ != *address++) validated = false;
        }
        return validated;
    }
*/

    void parseLibrary( const string& libName ) {
        auto libraryName = toUpper( libName );
        if (patchedLibraries.count( libraryName ) == 0)  {
            if (monitorLog( ParseLibrary )) { monitorLog() << L"Parsing " << ((0 < libraryName.size()) ? widen( libraryName.c_str() ) : L"program executable" )<< record; }
            // ToDo: Picking up original may not be necessary if parseLibrary is only called when no patches are in effect.
            auto load = original( "LoadLibraryExA" );
            if (load == nullptr) load = (PatchFunction)LoadLibraryExA;
            HMODULE library = ((TypeLoadLibraryExA)load)( libraryName.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
            patchedLibraries[ libraryName ] = library;
            static uint64_t Ordinal = 0x8000000000000000;
            static uint64_t Last = 0xFFFF;
            HMODULE imageBase = GetModuleHandleA( (0 < libraryName.size()) ? libraryName.c_str() : nullptr );
            IMAGE_NT_HEADERS* imageHeaders = (IMAGE_NT_HEADERS*)((char*)imageBase + ((IMAGE_DOS_HEADER*)imageBase)->e_lfanew);
/*
            if (library != nullptr) {
                // Inspect exported function list of this library...
                IMAGE_DATA_DIRECTORY exportsData = imageHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ];
                IMAGE_EXPORT_DIRECTORY* exportsDirectory = (IMAGE_EXPORT_DIRECTORY*)(exportsData.VirtualAddress + (char*)imageBase);
                DWORD* exportFunctionRA = (DWORD*)ImageRvaToVa( imageHeaders, imageBase, exportsDirectory->AddressOfFunctions, nullptr );
                DWORD* exportNameRA = (DWORD*)ImageRvaToVa( imageHeaders, imageBase, exportsDirectory->AddressOfNames, nullptr );
                for (int index = 0; index < exportsDirectory->NumberOfFunctions; index++) {
                    PatchFunction exportFunction = (PatchFunction)ImageRvaToVa( imageHeaders, imageBase, *exportFunctionRA, nullptr );
                    const char* exportName = (const char*)ImageRvaToVa( imageHeaders, imageBase, *exportNameRA, nullptr );
                    string name( exportName );
                    if (monitorLog( ImportedFunction )) { monitorLog() << L"    Exported function " << widen( name ) << record; }
                    if (0 < registeredPatches.count( name )) {
                        if (validateNopSignature( exportFunction )) {
                            if (monitorLog( PatchedFunction )) { monitorLog() << L"      Located hot patchable exported patch function " << widen( name ) << record; }
                        } else {
                            if (monitorLog( PatchedFunction )) { monitorLog() << L"      Found exported patch function " << widen( name ) << record; }
                        }
                    }
                    ++exportFunctionRA;
                    ++exportNameRA;
                }
            }
*/
            IMAGE_DATA_DIRECTORY importsDirectory = imageHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ];
            IMAGE_IMPORT_DESCRIPTOR* importDescriptor = (IMAGE_IMPORT_DESCRIPTOR*)(importsDirectory.VirtualAddress + (char*)imageBase);
            while ((importDescriptor->Name != 0) && ((importDescriptor->Name & Ordinal) == 0) && (importDescriptor->Name != Last)) {
                string importLibName( (const char*)(importDescriptor->Name + (char*)imageBase) );
                string importLibraryName = toUpper( importLibName );
                //if (patchedLibraries.count( importLibraryName ) == 0) {
                    parseLibrary( importLibraryName );
                    IMAGE_THUNK_DATA* originalFirstThunk = (IMAGE_THUNK_DATA*)((char*)imageBase + importDescriptor->OriginalFirstThunk);
                    IMAGE_THUNK_DATA* firstThunk = (IMAGE_THUNK_DATA*)((char*)imageBase + importDescriptor->FirstThunk);
                    while ((originalFirstThunk->u1.AddressOfData != 0) && ((originalFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) == 0)) {
                        IMAGE_IMPORT_BY_NAME* importName = (IMAGE_IMPORT_BY_NAME*)((char*)imageBase + originalFirstThunk->u1.AddressOfData);
                        string name( (char *)importName->Name );
                        if (monitorLog( ImportedFunction )) { monitorLog() << L"    Imported function " << widen( name ) << record; }
                        DWORD protection = 0;
                        if (0 < registeredPatches.count( name )) {
                            PatchFunction function = registeredPatches[ name ];
                            Patch& patchData = functionToPatch[ function ];
                            //if (patchData.original == nullptr) {
                                if (monitorLog( PatchedFunction )) { monitorLog() << L"      Located IAT patch function " << widen( name ) << record; }
                                patchData.library = &(patchedLibraries.find( importLibraryName )->first);
                                patchData.address = reinterpret_cast<PatchFunction*>(&firstThunk->u1.Function);
                                patchData.original = *patchData.address;
                            //}
                        }
                        ++originalFirstThunk;
                        ++firstThunk;
                    }
                //}
                importDescriptor++;
            }
        }
    } // void parseLibrary( const string& libName )
    void parseLibrary( const wstring& libName, bool force ) { parseLibrary( narrow( libName ) ); }

    void registerPatch( std::string name, PatchFunction function ) {
        static const char* signature = "void registerPatch( std::string name, PatchFunction function, std::string library )";
        if (0 < registeredPatches.count( name )) throw signature + string( " - Function " ) + name + string( " already registered!" );
        registeredPatches[ name ] = function;
        // ToDo: Consider what to do with library name case...
        if (monitorLog( RegisteredFunctions )) monitorLog() << L"Registered function " << widen( name ) << record;;

    }
    void unregisterPatch( std::string name ) {
        static const char* signature = "void unregisterPatch( std::string name )";
        if (registeredPatches.count( name ) == 0) throw signature + string( " - Function " ) + name + string( " not registred!" );
        registeredPatches.erase( name );
        if (monitorLog( RegisteredFunctions )) monitorLog() << L"Unregistered function " << widen( name ) << record;;
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
        if (monitorLog( ParseLibrary )) monitorLog() << L"Parsing libraries..." << record;
        parseLibrary( "" );
        if (monitorLog( ParseLibrary )) monitorLog() << L"Done Parsing libraries..." << record;
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

