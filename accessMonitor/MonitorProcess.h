#ifndef ACCESS_MONITOR_MONITOR_PROCESS_H
#define ACCESS_MONITOR_MONITOR_PROCESS_H

#include "Monitor.h"

namespace AccessMonitor {

    static const unsigned long MaxFileName = 1024;
    
    extern Log monitorEvents;

    // Start monitoring within current process
    // Spawned processes and threads will also be monitored.
    void startMonitoringProcess();
    // Stop monitoring on current  process
    void stopMonitoringProcess();

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_PROCESS_H
