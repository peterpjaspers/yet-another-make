#ifndef ACCESS_MONITOR_MONITOR_FILES_H
#define ACCESS_MONITOR_MONITOR_FILES_H

namespace AccessMonitor {
    // Register patches for OS file access functions
    void registerFileAccess();
    // Unregister patches for OS file access functions
    void unregisterFileAccess();
}

#endif // ACCESS_MONITOR_MONITOR_FILES_H
