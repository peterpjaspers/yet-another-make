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
        auto session( Session::current() );
        return new LogFile( monitorDebugPath( session->directory(), CurrentProcessID(), session->id() ), logTimes, logIntervals );
    }
    LogFile& debugLog() {
        static const char* signature = "LogFile& debugLog()";
        auto log = Session::current()->debugLog();
        if (log == nullptr) throw string( signature ) + " - No debug log defined for session";
        return( *log );
    }
    bool debugLog( const LogAspects aspects ) {
        auto session( Session::current() );
        if (session == nullptr) return false;
        auto log = session->debugLog();
        if (log == nullptr) return false;
        return (*log)( aspects );
    }
    LogRecord& debugRecord() { return debugLog()(); }

    LogFile* createEventLog() {
        auto session( Session::current() );
        return new LogFile( monitorEventsPath( session->directory(), CurrentProcessID(), session->id() ) );
    }
    bool recordingEvents() {
        auto session( Session::current() );
        if (session == nullptr) return false;
        auto log = session->eventLog();
        if (log == nullptr) return false;
        return true;
    }
    LogFile& eventLog() {
        static const char* signature = "LogFile& eventLog()";
        auto log = Session::current()->eventLog();
        if (log == nullptr) throw string( signature ) + " - No event log defined for session";
        return( *log );
    }
    LogRecord& eventRecord() { return eventLog()(); }

} // namespace AccessMonitor
