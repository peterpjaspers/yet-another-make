#ifndef ACCESS_MONITOR_MONITOR_LOGGING_H
#define ACCESS_MONITOR_MONITOR_LOGGING_H

#include "LogFile.h"

namespace AccessMonitor {

    static const unsigned long MaxFileName = 1024;
    
    LogFile* createDebugLog( bool logTimes = true, bool logIntervals = false );
    LogFile& debugLog();
    bool debugLog( const LogAspects aspects );
    LogRecord& debugRecord();

    LogFile* createEventLog();
    LogFile& eventLog();
    bool recordingEvents();
    LogRecord& eventRecord();

    enum MonitorLogAspects {
        RegisteredFunction  = (1 << 1),
        PatchedFunction     = (1 << 2),
        PatchExecution      = (1 << 3),
        FileAccesses        = (1 << 4),
        WriteTime           = (1 << 5),
    };

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_LOGGING_H
