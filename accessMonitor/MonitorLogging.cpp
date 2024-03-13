#include "MonitorLogging.h"
#include "FileNaming.h"

#include <map>

using namespace std;

// ToDo: Define debug logs the same as for event logs...

namespace AccessMonitor {

    namespace {
        // Monitor debug log for each session
        map<SessionID,Log> debugLogs;
        // Monitor event logs for each session
        map<SessionID,Log> eventLogs;
    }

    void createDebugLog() {
        static const char* signature = "void createDebugLog()";
        auto session = CurrentSessionID();
        if (0 < debugLogs.count( session )) throw string( signature ) + " - Debug log already defined for session";
        debugLogs[ session ] = Log( "MonitorLog", session, CurrentProcessID(), true );
    }
    void closeDebugLog() {
        debugLog().close();
        debugLogs.erase( CurrentSessionID() );
    }
    Log& debugLog() {
        static const char* signature = "Log& debugLog()";
        auto session = CurrentSessionID();
        if (debugLogs.count( session ) == 0) throw string( signature ) + " - No debug log defined for session";
        return debugLogs[ session ];
    }
    bool debugLog( const LogAspects aspects ) {
        if (!SessionDefined()) return false;
        if (debugLogs.count( CurrentSessionID() ) == 0) return false;
        return debugLog()( aspects );
    }
    LogRecord& debugRecord() { return debugLog()(); }

    void createEventLog() {
        static const char* signature = "void eventRecord( SessionID session )";
        auto session = CurrentSessionID();
        if (0 < eventLogs.count( session )) throw string( signature ) + " - Event log already defined for session";
        eventLogs[ session ] = Log( monitorEventsPath( CurrentProcessID() ) );
    }
    void closeEventLog() {
        eventLog().close();
        eventLogs.erase( CurrentSessionID() );
    }
    bool recordingEvents() {
        if (!SessionDefined()) return false;
        if (eventLogs.count( CurrentSessionID() ) == 0) return false;
        return true;
    }
    Log& eventLog() {
        static const char* signature = "Log& eventLog()";
        auto session = CurrentSessionID();
        if (eventLogs.count( session ) == 0) throw string( signature ) + " - No event log defined for session";
        return eventLogs[ session ];
    }
    LogRecord& eventRecord() { return eventLog()(); }

} // namespace AccessMonitor
