#include "Patch.h"
#include "Monitor.h"
#include "Log.h"

#include <windows.h>

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
        set< string > patchedLibraries;
        map< string, PatchFunction > registeredPatches;
        map< PatchFunction, Patch > functionToPatch;
        bool librariesPatched = false;

    }

    void registerPatch( std::string name, PatchFunction function ) {
        static const char* signature = "void registerPatch( std::string name, PatchFunction function )";
        if (0 < registeredPatches.count( name )) throw signature + string( " - Function " ) + name + string( " already registered!" );
        registeredPatches[ name ] = function;
        if(logging( Normal )) { log() << L"Registered function " << widen( name ) << endLine; };

    }
    void unregisterPatch( std::string name ) {
        static const char* signature = "void unregisterPatch( std::string name )";
        if (registeredPatches.count( name ) == 0) throw signature + string( " - Function " ) + name + string( " not registred!" );
        registeredPatches.erase( name );
        if(logging( Normal )) { log() << L"Unregistered function " << widen( name ) << endLine; };
    }

    void patchImportTable( PatchFunction* address, PatchFunction function ) {
        DWORD protection = 0;
        VirtualProtect((void*)address, sizeof( PatchFunction ), PAGE_READWRITE, &protection );
        *address = function;
        VirtualProtect((void*)address, sizeof( PatchFunction ), protection, nullptr );
    }

    void patchLibrary( bool patch, const string& libraryName ) {
        if (patchedLibraries.count( libraryName ) == 0) {
            if(logging( Normal )) { log() << wstring((patch) ? L"Patching " : L"Unpatching ") << ((0 < libraryName.size()) ? widen( libraryName.c_str() ) : L"program executable" )<< endLine; }
            patchedLibraries.insert( libraryName );
            static uint64_t Ordinal = 0x8000000000000000;
            static uint64_t Last = 0xFFFF;
            HMODULE imageBase = GetModuleHandleA( (0 < libraryName.size()) ? libraryName.c_str() : nullptr );
            IMAGE_NT_HEADERS* imageHeaders = (IMAGE_NT_HEADERS*)((char*)imageBase + ((IMAGE_DOS_HEADER*)imageBase)->e_lfanew);
            IMAGE_DATA_DIRECTORY importsDirectory = imageHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ];
            IMAGE_IMPORT_DESCRIPTOR* importDescriptor = (IMAGE_IMPORT_DESCRIPTOR*)(importsDirectory.VirtualAddress + (char*)imageBase);
            while ((importDescriptor->Name != 0) && ((importDescriptor->Name & Ordinal) == 0) && (importDescriptor->Name != Last)) {
                string importLibraryName( (const char*)(importDescriptor->Name + (char*)imageBase) );
                if (patchedLibraries.count( importLibraryName ) == 0) {
                    patchLibrary( patch, importLibraryName );
                    HMODULE library = LoadLibraryA( importLibraryName.c_str() );
                    if ( library ) {
                        IMAGE_THUNK_DATA* originalFirstThunk = (IMAGE_THUNK_DATA*)((char*)imageBase + importDescriptor->OriginalFirstThunk);
                        IMAGE_THUNK_DATA* firstThunk = (IMAGE_THUNK_DATA*)((char*)imageBase + importDescriptor->FirstThunk);
                        while ((originalFirstThunk->u1.AddressOfData != 0) && ((originalFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) == 0)) {
                            IMAGE_IMPORT_BY_NAME* importName = (IMAGE_IMPORT_BY_NAME*)((char*)imageBase + originalFirstThunk->u1.AddressOfData);
                            string name( (char *)importName->Name );
                            if (logging( Verbose )) { log() << L"    Imported function " << widen( name ) << endLine; }
                            DWORD protection = 0;
                            if (0 < registeredPatches.count( name )) {
                                PatchFunction function = registeredPatches[ name ];
                                Patch& patchData = functionToPatch[ function ];
                                if (patch && (patchData.original == nullptr)) {
                                    if(logging( Terse )) { log() << L"      Patched function " << widen( name ) << " in " << widen( importLibraryName )<< endLine; }
                                    patchData.library = &(*patchedLibraries.find( importLibraryName ));
                                    patchData.address = reinterpret_cast<PatchFunction*>(&firstThunk->u1.Function);
                                    patchData.original = *patchData.address;
                                    patchImportTable( patchData.address, function );
                                } else if (!patch && (patchData.original != nullptr)) {
                                    // Restore to original only if function was not re-patched by some other party
                                    if (function == *patchData.address) {
                                        if(logging( Terse )) { log() << L"      Unpatched function " << widen( name ) << " in " << widen( importLibraryName ) << endLine; }
                                        patchImportTable( patchData.address, patchData.original );
                                        patchData.original = nullptr;
                                    } else {
                                        if(logging( Terse )) { log() << L"      Function " << widen( name ) << " in " << widen( importLibraryName ) << L" was repatched!" << endLine; }
                                    }
                                }
                            }
                            ++originalFirstThunk;
                            ++firstThunk;
                        }
                    }
                }
                importDescriptor++;
            }    
        }
    } // void patchLibrary( bool patch, const string& libraryName, set<string>& patchedLibraries )

    void patchLibraries( bool patch ) {
        static const char* signature = "void patchLibraries( bool patch )";
        if(logging( Normal )) { log() << wstring((patch) ? L"Patching" : L"Unpatching") << L" libraries..." << endLine; }
        if (librariesPatched && patch) throw string( signature ) + string( " - Libraries already patched!" );
        if (!librariesPatched && !patch) throw string( signature ) + string( " - Libraries not patched!" );
        patchLibrary( patch, "" );
        librariesPatched = patch;
    }

    PatchFunction original( std::string name ) { return functionToPatch[ registeredPatches[ name ] ].original; }
    PatchFunction original( PatchFunction function ) { return functionToPatch[ function ].original; }
    PatchFunction patched( std::string name ) { return registeredPatches[ name ]; }
    const std::string& patchedLibrary( std::string name ) { return *functionToPatch[ registeredPatches[ name ] ].library; }
    const std::string& patchedLibrary( PatchFunction function ){ return *functionToPatch[ function ].library; }

    bool repatched( std::string name ) {
        PatchFunction function = patched( name );
        Patch& patch = functionToPatch[ function ];
        return( (*patch.address != patch.original) && (*patch.address != function) );
    }

    void patch() {
        patchedLibraries.clear();
        functionToPatch.clear();
        patchLibraries( true );
    }
    void unpatch() {
        patchedLibraries.clear();
        patchLibraries( false );
    }

} // namespace AccessMonitor

