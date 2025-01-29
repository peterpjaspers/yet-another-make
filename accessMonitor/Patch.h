#ifndef ACCESS_MONITOR_PATCH_H
#define ACCESS_MONITOR_PATCH_H

#include <string>

namespace AccessMonitor {

    typedef void (*PatchFunction)();
    typedef unsigned long PatchIndex;
    const PatchIndex MaxPatchIndex( 100 );

    struct Registration {
        const char* library;
        const char* name;
        PatchFunction patch;
        PatchIndex index;
    };

    // Register a patch function for a function in a library.
    // Returns true if patch function was registered.
    // Returns false if library could not be accessed or function is not present in library.
    bool registerPatch( const std::string& library, const std::string& name, const PatchFunction function, const PatchIndex index );
    // Unregister a patch for a function
    void unregisterPatch( const std::string& name );   

    // Return original function associated with a patch index
    PatchFunction original( const PatchIndex index );
    // Return original function associated with a named function
    PatchFunction original( const std::string& functionName );

    // Patch libraries with all registered patches
    void patch();
    // Un-patch libraries with all registered patches
    void unpatch();

    // Patches may be suppressed to avoid recursive calls to patch functions.
    // Functions return true if repatched or unpatched respectively.
    // Re-patch a specific (suppressed) patch function.
    bool patchFunction( const PatchIndex index );
    // Unpatch (suppress patch of) a specific patch function.
    bool unpatchFunction( const PatchIndex index );
    
    // Return original function associated with a patch function and its index.
    // The template argument is the type (signature) of the (original and patch) function.
    template<class T>
    T patchOriginal( T function, PatchIndex index ) { 
        return reinterpret_cast<T>( original( index ) ); 
    }

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_PATCH_H
