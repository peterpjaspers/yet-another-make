#include "Patch.h"
#include "MonitorLogging.h"
#include <windows.h>
#include "../detours/inc/detours.h"

#include <map>

using namespace std;

namespace AccessMonitor {

    namespace {

        struct Patch {
            PatchFunction           address;        // Address of function to patch
            mutable PatchFunction   original;       // Address of trampoline to original function
            Patch() : address( nullptr ), original( nullptr ) {}
            Patch( PatchFunction function ) : address( function ), original( nullptr ) {}
        };

        map< string, PatchFunction > registeredPatches;
        map< PatchFunction, Patch > functionToPatch;
        bool librariesPatched = false;

    }

    // Registered functions may not actually be present (imported) in an executable or DLL.
    // In that case, patchFunction and unpatchFunction do nothing.
    bool patchFunction( const PatchFunction function ) {
        const auto entry = functionToPatch.find( function );
        if (entry != functionToPatch.end()) {
            const auto& patch = entry->second;
            patch.original = patch.address;
            LONG error = DetourAttach( (void**)&patch.original, (void*)function );
            if (error == NO_ERROR) return true;
            // Not able to patch requested function, default to original function address
            if (debugLog( PatchedFunction )) { debugRecord() << L"      DetourAttach failed with " << error << record; }
        }
        return false;
    }
    bool unpatchFunction( const PatchFunction function ) {
        const auto entry = functionToPatch.find( function );
        if (entry != functionToPatch.end()) {
            const auto& patch = entry->second;
            LONG error = DetourDetach( (void**)&patch.original, (void*)function );
            patch.original = nullptr;
            if (error == NO_ERROR) return true;
            if (debugLog( PatchedFunction )) { debugRecord() << L"      DetourDetach failed with " << error << record; }
        }
        return false;
    }
    void patchAll() {
        for ( auto function : registeredPatches ) {
            if (patchFunction( function.second )) {
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Patched function " << widen( function.first ) << record; }
            } else if (0 < functionToPatch.count( function.second )) {
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Unable to patch function " << widen( function.first ) << record; }
            }
        }
    }
    void unpatchAll() {
        for ( auto function : registeredPatches ) {
            if (unpatchFunction( function.second )) {
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Unpatched function " << widen( function.first ) << record; }
            } else if (0 < functionToPatch.count( function.second )) {
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Unable to unpatch function " << widen( function.first ) << record; }
            }
        }        
    }

    bool registerPatch( std::string library, std::string name, PatchFunction function ) {
        static const char* signature = "void registerPatch( std::string name, PatchFunction function, std::string library )";
        if (0 < registeredPatches.count( name )) throw string( signature ) + string( " - Function " ) + name + string( " already registered!" );
        HMODULE handle = GetModuleHandleA( library.c_str() );
        if (handle != nullptr) {
            auto address = reinterpret_cast<PatchFunction>( GetProcAddress( handle, name.c_str() ) );
            if (address != nullptr) {
                // Named function is present in library...
                registeredPatches[ name ] = function;
                functionToPatch[ function ] = Patch( address );
                if (debugLog( RegisteredFunction )) debugRecord() << L"Registered function " << widen( name ) << L" in library " << widen( library ) << record;
                return true;
            } else {
                if (debugLog( RegisteredFunction )) debugRecord() << L"Function " << widen( name ) << L" not present in library " << widen( library ) << L" [ " << GetLastError() << " ]" << record;;
            }
        } else {
            if (debugLog( RegisteredFunction )) debugRecord() << L"Library " << widen( library ) << L" could not be accessed [ " << GetLastError() << " ]" << record;;
        }
        return false;
    }
    void unregisterPatch( std::string name ) {
        static const char* signature = "void unregisterPatch( std::string name )";
        if (registeredPatches.count( name ) == 0) throw runtime_error( string( signature ) + " - Function " + name + " not registred!" );
        auto function = registeredPatches[ name ] ;
        registeredPatches.erase( name );
        functionToPatch.erase( function );
        if (debugLog( RegisteredFunction )) debugRecord() << L"Unregistered function " << widen( name ) << record;;
    }

    PatchFunction original( PatchFunction function ) {
        const auto entry = functionToPatch.find( function );
        if (entry == functionToPatch.end()) return nullptr;
        const auto& patch = entry->second;
        if (patch.original != nullptr) return patch.original;
        return patch.address;
    }
    PatchFunction original( std::string name ) {
        static const char* signature = "PatchFunction original( std::string name )";
        auto function = registeredPatches[ name ];
        if (function == nullptr) throw runtime_error( string( signature ) + " - Function " + name + " not registred!" );
        return original( function );
    }

    void patch() {
        static const char* signature = "void patch()";
        if (librariesPatched) throw runtime_error( string( signature ) + " - Libraries already patched!" );
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        patchAll();
        DetourTransactionCommit();
        librariesPatched = true;
    }
    void unpatch() {
        static const char* signature = "void unpatch()";
        if (!librariesPatched) throw runtime_error( string( signature ) + " - Libraries not patched!" );
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        unpatchAll();
        DetourTransactionCommit();
        functionToPatch.clear();
        librariesPatched = false;
    }

} // namespace AccessMonitor

