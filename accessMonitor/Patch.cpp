#include "Patch.h"
#include "MonitorLogging.h"

#include <windows.h>
#include "../detours/inc/detours.h"

#include <map>

using namespace std;

namespace AccessMonitor {

    namespace {

        struct Patch {
            PatchFunction   patch;          // Address of patch function
            PatchFunction   target;         // Address of target function to patch
            PatchFunction   original;       // Address of trampoline to target function
            Patch() : patch( nullptr ), target( nullptr ), original( nullptr ) {}
            Patch( const PatchFunction patchFunction, const PatchFunction targetFunction  ) : patch( patchFunction ), target( targetFunction ), original( nullptr ) {}
            Patch( const Patch& patch ) : patch( patch.patch), target( patch.target ), original( patch.original ) {}
        };

        struct RegisteredPatch {
            PatchFunction   function;
            PatchIndex      index;
            RegisteredPatch() : function( nullptr ), index( MaxPatchIndex ) {}
            RegisteredPatch( const PatchFunction patchFunction, const PatchIndex patchIndex ) : function( patchFunction ), index( patchIndex ) {}
            RegisteredPatch( const RegisteredPatch& registration ) : function( registration.function ), index( registration.index ) {}
        };

        map< string, RegisteredPatch > registeredPatches;
        Patch indexToPatch[ MaxPatchIndex ];
        bool librariesPatched = false;

        inline void testFunctionFailure( const char* signature, const char* function, long error ) {
            if (error != NO_ERROR) throw runtime_error( string( signature ) + " - " + function + " failed : " + to_string( error ) );
        }
    }

    // Registered functions may not actually be present (imported) in an executable or DLL.
    // In that case, patchFunction and unpatchFunction do nothing.
    bool patchFunction( const PatchIndex index ) {
        const char* signature( "bool patchFunction( const PatchFunction function )" );
        auto& patch( indexToPatch[ index ] );
        if (patch.target != nullptr ) {
            patch.original = patch.target;
            long error = DetourAttach( (void**)&patch.original, (void*)patch.patch );
            testFunctionFailure( signature, "DetourAttach", error );
            return true;
        }
        return false;
    }
    bool unpatchFunction( const PatchIndex index ) {
        const char* signature( "bool unpatchFunction( const PatchFunction function )" );
        auto& patch( indexToPatch[ index ] );
        if (patch.target != nullptr ) {
            long error = DetourDetach( (void**)&patch.original, (void*)patch.patch );
            patch.original = nullptr;
            testFunctionFailure( signature, "DetourDettach", error );
            return true;
        }
        return false;
    }
    void patchAll() {
        for ( auto entry : registeredPatches ) {
            auto& name( entry.first );
            auto& registration( entry.second );
            if (!patchFunction( registration.index )) {
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Unable to patch function " << widen( name ) << record; }
            }
        }
    }
    void unpatchAll() {
        for ( auto entry : registeredPatches ) {
            auto& registration( entry.second );
            auto& name( entry.first );
            if (!unpatchFunction( registration.index )) {
                if (debugLog( PatchedFunction )) { debugRecord() << L"      Unable to unpatch function " << widen( name ) << record; }
            }
        }        
    }

    bool registerPatch( const std::string& library, const std::string& name, const PatchFunction function, const PatchIndex index ) {
        static const char* signature = "PatchFunction original( std::string name )";
        if (registeredPatches.count( name ) != 0) throw runtime_error( string( signature ) + " - Function " + name + " already registered!" );
        if (MaxPatchIndex <= index) throw runtime_error( string( signature ) + " - Function " + name + " index " + to_string( index ) + " exceeds maximum!" );
        auto& patch( indexToPatch[ index ] );
        if (patch.patch != nullptr) throw runtime_error( string( signature ) + " - Function " + name + " index " + to_string( index ) + " already in use!" );
        registeredPatches[ name ] = { function, index };
        patch.patch = function;
        HMODULE handle = GetModuleHandleA( library.c_str() );
        if (handle != nullptr) {
            auto target = reinterpret_cast<PatchFunction>( GetProcAddress( handle, name.c_str() ) );
            if (target != nullptr) {
                // Named function is present in library...
                patch.target = target;
                if (debugLog( RegisteredFunction )) debugRecord() << L"Registered function " << widen( name ) << L" at index " << index << L" in library " << widen( library ) << record;
                return true;
            } else {
                if (debugLog( RegisteredFunction )) debugRecord() << L"Function " << widen( name ) << L" not present in library " << widen( library ) << L" [ " << GetLastError() << " ]" << record;
            }
        } else {
            if (debugLog( RegisteredFunction )) debugRecord() << L"Library " << widen( library ) << L" could not be accessed [ " << GetLastError() << " ]" << record;
        }
        return false;
    }
    void unregisterPatch( const std::string& name ) {
        static const char* signature = "void unregisterPatch( const std::string& name )";
        if (registeredPatches.count( name ) == 0) throw runtime_error( string( signature ) + " - Function " + name + " not registered!" );
        const auto& registration( registeredPatches[ name ] );
        auto& patch( indexToPatch[ registration.index ] );
        registeredPatches.erase( name );
        patch = { nullptr, nullptr };
        if (debugLog( RegisteredFunction )) debugRecord() << L"Unregistered function " << widen( name ) << record;
    }

    PatchFunction original( const PatchIndex index ) {
        const auto& patch = indexToPatch[ index ];
        if (patch.target == nullptr) return nullptr;
        if (patch.original != nullptr) return patch.original;
        return patch.target;
    }
    PatchFunction original( const std::string& name ) {
        static const char* signature = "PatchFunction original( std::string name )";
        if (registeredPatches.count( name ) == 0) throw runtime_error( string( signature ) + " - Function " + name + " not registred!" );
        const auto& registration = registeredPatches[ name ];
        return original( registration.index );
    }

    void patch() {
        static const char* signature = "void patch()";
        if (librariesPatched) throw runtime_error( string( signature ) + " - Libraries already patched!" );
        auto retained = DetourSetRetainRegions( true );
        auto ignored = DetourSetIgnoreTooSmall( false );
        long error( 0 );
        error = DetourTransactionBegin();
        testFunctionFailure( signature, "DetourTransactionBegin", error );
        error = DetourUpdateThread(GetCurrentThread());
        testFunctionFailure( signature, "DetourUpdateThread", error );
        patchAll();
        error = DetourTransactionCommit();
        testFunctionFailure( signature, "DetourTransactionCommit", error );
        librariesPatched = true;
    }
    void unpatch() {
        static const char* signature = "void unpatch()";
        if (!librariesPatched) throw runtime_error( string( signature ) + " - Libraries not patched!" );
        long error( 0 );
        error = DetourTransactionBegin();
        testFunctionFailure( signature, "DetourTransactionBegin", error );
        error = DetourUpdateThread(GetCurrentThread());
        testFunctionFailure( signature, "DetourUpdateThread", error );
        unpatchAll();
        error = DetourTransactionCommit();
        testFunctionFailure( signature, "DetourTransactionCommit", error );
        librariesPatched = false;
    }

} // namespace AccessMonitor

