#ifndef ACCESS_MONITOR_MONITOR_H
#define ACCESS_MONITOR_MONITOR_H

#include "Process.h"

namespace AccessMonitor {

    typedef unsigned long ProcessID;
    
    // Start monitoring file access.
    // Spawned processes and threads will also be monitored.
    void startMonitoring();

    // Start monitoring file access in identified process.
    // Processes and threads spawned by the process will also be monitored.
    void startMonitoring( ProcessID process );

    // Stop monitoring file access.
    void stopMonitoring();

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_H
