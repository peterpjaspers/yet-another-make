#ifndef MONITOR_FILES_H
#define MONITOR_FILES_H

namespace AccessMonitor {
    // Register patches for OS file access functions
    void registerFileAccess();
    // Unregister patches for OS file access functions
    void unregisterFileAccess();
}

#endif // MONITOR_FILES_H
