#ifndef MONITOR_THREADS_AND_PROCESSES_H
#define MONITOR_THREADS_AND_PROCESSES_H

namespace AccessMonitor {
    // Register patches for OS process creation functions
    void registerProcessesAndThreads();
    // Unregister patches for OS process creation functions
    void unregisterProcessCreation();
}

#endif // MONITOR_THREADS_AND_PROCESSES_H
