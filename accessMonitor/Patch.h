#ifndef ACCESS_MONITOR_PATCH_H
#define ACCESS_MONITOR_PATCH_H

#include <string>
#include <iostream>

namespace AccessMonitor {

    typedef void (*PatchFunction)();

    // Register a patch for the named function
    void registerPatch( std::string name, PatchFunction function );
    // Unregister a patch for the named function
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
    
} // namespace AccessMonitor

#endif // ACCESS_MONITOR_PATCH_H
