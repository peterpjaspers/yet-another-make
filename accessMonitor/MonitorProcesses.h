#ifndef ACCESS_MONITOR_MONITOR_PROCESSES_H
#define ACCESS_MONITOR_MONITOR_PROCESSES_H

namespace AccessMonitor {
    // Register the patch dll module to allow for lookup
    // of the patch dll path.
    void setPatchDllModule(HINSTANCE dll);

    // Register patches for OS process creation functions
    void registerProcessAccess();
    // Unregister patches for OS process creation functions
    void unregisterProcessAccess();
}

#endif // ACCESS_MONITOR_MONITOR_PROCESSES_H
