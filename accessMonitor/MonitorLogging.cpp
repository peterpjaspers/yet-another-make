#include "MonitorLogging.h"
#include "FileNaming.h"
#include "Process.h"
#include "Session.h"
#include "LogFile.h"

#include <map>
#include <mutex>

using namespace std;

namespace AccessMonitor {

    LogFile* createDebugLog( bool logTimes, bool logIntervals ) {
        return new LogFile( monitorDebugPath( CurrentSessionDirectory(), CurrentProcessID(), CurrentSessionID() ), logTimes, logIntervals );
    }
    LogFile& debugLog() {
        static const char* signature = "LogFile& debugLog()";
        auto log = static_cast<LogFile*>(SessionDebugLog());
        if (log == nullptr) throw string( signature ) + " - No debug log defined for session";
        return( *log );
    }
    bool debugLog( const LogAspects aspects ) {
        auto log = SessionDebugLog();
        if (log == nullptr) return false;
        return (*log)( aspects );
    }
    LogRecord& debugRecord() { return debugLog()(); }

    LogFile* createEventLog() {
        return new LogFile( monitorEventsPath( CurrentSessionDirectory(), CurrentProcessID(), CurrentSessionID() ) );
    }
    bool recordingEvents() {
        auto log = SessionEventLog();
        if (log == nullptr) return false;
        return true;
    }
    LogFile& eventLog() {
        static const char* signature = "LogFile& eventLog()";
        auto log = SessionEventLog();
        if (log == nullptr) throw string( signature ) + " - No event log defined for session";
        return( *log );
    }
    LogRecord& eventRecord() { return eventLog()(); }

} // namespace AccessMonitor
