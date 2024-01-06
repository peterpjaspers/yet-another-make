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
    // Return patched function associated with a named function
    PatchFunction patched( std::string functionName );
    // Return patched library for a named function
    const std::string& patchedLibrary( std::string name );
    // Return patched library for a patched function
    const std::string& patchedLibrary( PatchFunction function );

    // Validate if a patch was re-patched by a third party
    bool repatched( std::string name );

    // Patch libraries with all registered patches
    void patch();
    // Un-patch libraries with all registered patches
    void unpatch();

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_PATCH_H
