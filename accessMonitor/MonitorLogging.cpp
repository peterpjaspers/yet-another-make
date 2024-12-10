#include "MonitorLogging.h"
#include "FileNaming.h"
#include "Process.h"
#include "Session.h"
#include "LogFile.h"

#include <map>
#include <mutex>

using namespace std;
using namespace filesystem;

namespace AccessMonitor {

#ifdef _DEBUG_ACCESS_MONITOR
    LogFile* createDebugLog( const path& dir, unsigned long code, bool logTimes, bool logIntervals ) {
        return new LogFile( monitorDebugPath( dir, CurrentProcessID(), code ), logTimes, logIntervals );
    }
    LogFile& debugLog() {
        static const char* signature = "LogFile& debugLog()";
        auto session( Session::current() );
        if (session == nullptr) throw runtime_error( string( signature ) + " - No debug log defined for session" );
        auto log( session->debugLog() );
        if (log == nullptr) throw runtime_error( string( signature ) + " - No debug log defined for session" );
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
#else
    LogFile* createDebugLog( const path& dir, unsigned long code, bool logTimes, bool logIntervals ) { return nullptr; }
    LogFile& debugLog() {
        static const char* signature = "LogFile& debugLog()";
        throw runtime_error( string( signature ) + " - Debug log undefined for optimized session" );
    }
    LogRecord& debugRecord() {
        static const char* signature = "LogFile& debugLog()";
        throw runtime_error( string( signature ) + " - Debug log undefined for optimized session" );
    }
#endif

    LogFile* createEventLog( const path& dir, unsigned long code ) {
        return new LogFile( monitorEventsPath( dir, CurrentProcessID(), code ) );
    }
    bool recordingEvents() {
        auto session( Session::current() );
        if (session == nullptr) return false;
        auto log( session->eventLog() );
        if (log == nullptr) return false;
        return true;
    }
    LogFile& eventLog() {
        static const char* signature = "LogFile& eventLog()";
        auto log( Session::current()->eventLog() );
        if (log == nullptr) throw runtime_error( string( signature ) + " - No event log defined for session" );
        return( *log );
    }
    LogRecord& eventRecord() { return eventLog()(); }

} // namespace AccessMonitor
