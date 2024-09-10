#include "Patch.h"
#include "MonitorLogging.h"
#include <windows.h>
#include "../detours/inc/detours.h"

#include <map>

#pragma comment(lib, "detours.lib")

// ToDo: May need to patch functions other than those exported by kernel32.dll (e.g. ntdll.dll)

using namespace std;

namespace AccessMonitor {

    namespace {

        map< string, PatchFunction > registeredPatches;
        map< PatchFunction, PatchFunction > functionToOriginal;
        bool librariesPatched = false;

    }

    // Registered functions may not actually be present (imported) in an executable or DLL.
    // In that case, patchFunction and unpatchFunction do nothing.
    bool patchFunction( const PatchFunction function ) {
        if (0 < functionToOriginal.count( function )) {
            LONG error = DetourAttach( (void**)&functionToOriginal[ function ], (void*)function );
            if (error == NO_ERROR) return true;
            // Not able to patch requested function, remove it
            functionToOriginal.erase( function );
            if (debugLog( PatchedFunction )) { debugRecord() << L"      DetourAttach failed with " << error << record; }
        }
        return false;
    }
    bool unpatchFunction( const PatchFunction function ) {
        if (0 < functionToOriginal.count( function )) {
            LONG error = DetourDetach( (void**)&functionToOriginal[ function ], (void*)function );
            if (error == NO_ERROR) return true;
            if (debugLog( PatchedFunction )) { debugRecord() << L"      DetourDetach failed with " << error << record; }
        }
        return false;
    }
    void patchAll() {
        for ( auto function : registeredPatches ) {
            if (patchFunction( function.second )) {
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Patched function " << widen( function.first ) << record; }
            } else if (0 < functionToOriginal.count( function.second )) {
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Unable to patch function " << widen( function.first ) << record; }
            }
        }
    }
    void unpatchAll() {
        for ( auto function : registeredPatches ) {
            if (unpatchFunction( function.second )) {
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Unpatched function " << widen( function.first ) << record; }
            } else if (0 < functionToOriginal.count( function.second )) {
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Unable to unpatch function " << widen( function.first ) << record; }
            }
        }        
    }

    void registerPatch( std::string name, PatchFunction function ) {
        static const char* signature = "void registerPatch( std::string name, PatchFunction function, std::string library )";
        if (0 < registeredPatches.count( name )) throw string( signature ) + string( " - Function " ) + name + string( " already registered!" );
        registeredPatches[ name ] = function;
        HMODULE handle = GetModuleHandleA( "kernel32.dll" );
        PatchFunction original = reinterpret_cast<PatchFunction>( GetProcAddress( handle, name.c_str() ) );
        if (original != nullptr) {
            // Registered function is actually present in this image...
            functionToOriginal[ function ] = original;
        }
        if (debugLog( RegisteredFunctions )) debugRecord() << L"Registered function " << widen( name ) << record;;

    }
    void unregisterPatch( std::string name ) {
        static const char* signature = "void unregisterPatch( std::string name )";
        if (registeredPatches.count( name ) == 0) throw string( signature ) + string( " - Function " ) + name + string( " not registred!" );
        PatchFunction function = registeredPatches[ name ] ;
        registeredPatches.erase( name );
        functionToOriginal.erase( function );
        if (debugLog( RegisteredFunctions )) debugRecord() << L"Unregistered function " << widen( name ) << record;;
    }

    PatchFunction original( PatchFunction function ) {
        if (functionToOriginal.count( function ) == 0) return nullptr;
        return functionToOriginal[ function ];
    }
    PatchFunction original( std::string name ) { return original( registeredPatches[ name ] ); }

    void patch() {
        static const char* signature = "void patch()";
        if (librariesPatched) throw string( signature ) + string( " - Libraries already patched!" );
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        patchAll();
        DetourTransactionCommit();
        librariesPatched = true;
    }
    void unpatch() {
        static const char* signature = "void unpatch()";
        if (!librariesPatched) throw string( signature ) + string( " - Libraries not patched!" );
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        unpatchAll();
        DetourTransactionCommit();
        functionToOriginal.clear();
        librariesPatched = false;
    }

} // namespace AccessMonitor

