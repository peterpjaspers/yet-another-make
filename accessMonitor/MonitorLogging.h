#ifndef ACCESS_MONITOR_MONITOR_LOGGING_H
#define ACCESS_MONITOR_MONITOR_LOGGING_H

#include "LogFile.h"

#include <filesystem>

namespace AccessMonitor {

    static const unsigned long MaxFileName = 1024;
    
    LogFile* createDebugLog( const std::filesystem::path& dir, unsigned long code, bool logTimes = true, bool logIntervals = false );
    LogFile& debugLog();
#ifdef _DEBUG_ACCESS_MONITOR
    bool debugLog( const LogAspects aspects );
#else
    // Supress debug logging (as fast as possible) when optimized...
    inline bool debugLog( const LogAspects aspects ) { return false; }
#endif
    LogRecord& debugRecord();

    LogFile* createEventLog( const std::filesystem::path& dir, unsigned long code );
    LogFile& eventLog();
    bool recordingEvents();
    LogRecord& eventRecord();

    enum MonitorLogAspects {
        General             = (1 << 1),
        RegisteredFunction  = (1 << 2),
        PatchedFunction     = (1 << 3),
        PatchExecution      = (1 << 4),
        FileAccesses        = (1 << 5),
        WriteTime           = (1 << 6),
    };

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_LOGGING_H
