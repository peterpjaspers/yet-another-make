#ifndef ACCESS_MONITOR_MONITOR_H
#define ACCESS_MONITOR_MONITOR_H

#include "Process.h"
#include "Log.h"

#include <fstream>

namespace AccessMonitor {

    enum MonitorLogAspects {
        RegisteredFunctions = (1 << 0),
        ParseLibrary        = (1 << 1),
        ExportedFunction    = (1 << 2),
        ImportedFunction    = (1 << 3),
        PatchedFunction     = (1 << 4),
        PatchExecution      = (1 << 5),
        FileAccess          = (1 << 6),
        WriteTime           = (1 << 7),
    };

    extern Log monitorLog;

    // Start monitoring file access.
    void startMonitoring();
    // Stop monitoring file access.
    void stopMonitoring();

    // Start monitoring on current process
    // Spawned processes and threads will also be monitored.
    void startMonitoringProcess();
    // Stop monitoring on current  process
    void stopMonitoringProcess();

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_H
