#ifndef ACCESS_MONITOR_PATCH_H
#define ACCESS_MONITOR_PATCH_H

#include <string>

namespace AccessMonitor {

    typedef void (*PatchFunction)();

    struct Registration {
        const char* library;
        const char* name;
        PatchFunction patch;
    };

    // Register a patch function for a function in a library.
    // Returns true if patch function was registered.
    // Returns false if library could not be accessed or function is not present in library.
    bool registerPatch( std::string library, std::string name, PatchFunction function );
    // Unregister a patch for a function
    void unregisterPatch( std::string name );   

    // Return original function associated with a named function
    PatchFunction original( std::string functionName );
    // Return original function associated with a patched function
    PatchFunction original( PatchFunction function );

    // Patch libraries with all registered patches
    void patch();
    // Un-patch libraries with all registered patches
    void unpatch();

    // Patches may be suppressed to avoid recursive calls to patch functions.
    // Functions return true if repatched or unpatched respectively.
    // Re-patch a specific (suppressed) patch function.
    bool patchFunction( const PatchFunction function );
    // Unpatch (suppress patch of) a specific patch function.
    bool unpatchFunction( const PatchFunction function );
    
    // Return original function associated with a patch function.
    // The template argument is the type (signature) of the (original) function.
    template<class T>
    T patchOriginal( T function ) { return reinterpret_cast<T>( original( reinterpret_cast<PatchFunction>( function ) ) ); }

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_PATCH_H
