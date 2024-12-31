#ifndef ACCESS_MONITOR_MONITOR_PROCESSES_H
#define ACCESS_MONITOR_MONITOR_PROCESSES_H

namespace AccessMonitor {
    // Register patches for OS process creation functions
    void registerProcessAccess();
    // Unregister patches for OS process creation functions
    void unregisterProcessAccess();
}

#endif // ACCESS_MONITOR_MONITOR_PROCESSES_H
