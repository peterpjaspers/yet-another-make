#ifndef ACCESS_MONITOR_MONITOR_LOGGING_H
#define ACCESS_MONITOR_MONITOR_LOGGING_H

#include "Log.h"

namespace AccessMonitor {

    static const unsigned long MaxFileName = 1024;
    
    void createDebugLog();
    void closeDebugLog();
    Log& debugLog();
    bool debugLog( const LogAspects aspects );
    LogRecord& debugRecord();

    void createEventLog();
    void closeEventLog();
    Log& eventLog();
    bool recordingEvents();
    LogRecord& eventRecord();

    enum MonitorLogAspects {
        RegisteredFunctions = (1 << 1),
        ParseLibrary        = (1 << 2),
        ExportedFunction    = (1 << 3),
        ImportedFunction    = (1 << 4),
        PatchedFunction     = (1 << 5),
        PatchExecution      = (1 << 6),
        FileAccesses        = (1 << 7),
        WriteTime           = (1 << 8),
    };

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_LOGGING_H
