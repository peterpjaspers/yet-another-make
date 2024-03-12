#ifndef ACCESS_MONITOR_MONITOR_LOGGING_H
#define ACCESS_MONITOR_MONITOR_LOGGING_H

#include "Log.h"

namespace AccessMonitor {

    static const unsigned long MaxFileName = 1024;
    
    extern Log monitorLog;

    void createEventLog( SessionID session );
    void closeEventLog( SessionID session );
    LogRecord& eventRecord();

    enum MonitorLogAspects {
        RegisteredFunctions = (1 << 0),
        ParseLibrary        = (1 << 1),
        ExportedFunction    = (1 << 2),
        ImportedFunction    = (1 << 3),
        PatchedFunction     = (1 << 4),
        PatchExecution      = (1 << 5),
        FileAccesses        = (1 << 6),
        WriteTime           = (1 << 7),
    };

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_LOGGING_H
