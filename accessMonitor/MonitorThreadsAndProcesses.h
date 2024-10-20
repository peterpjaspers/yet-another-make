#ifndef ACCESS_MONITOR_MONITOR_THREADS_AND_PROCESSES_H
#define ACCESS_MONITOR_MONITOR_THREADS_AND_PROCESSES_H

namespace AccessMonitor {
    // Register patches for OS process creation functions
    void registerProcessesAndThreads();
    // Unregister patches for OS process creation functions
    void unregisterProcessesAndThreads();
}

#endif // ACCESS_MONITOR_MONITOR_THREADS_AND_PROCESSES_H
