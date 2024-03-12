#include "MonitorLogging.h"
#include "FileNaming.h"

#include <map>

using namespace std;

namespace AccessMonitor {

    namespace {
        // Recorded monitor events for each session
        map<SessionID,Log> eventLogs;
    }

    // Monitor debug log
    Log monitorLog( "MonitorLog", CurrentProcessID(), true );

    void createEventLog( SessionID session ) {
        static const char* signature = "void eventRecord( SessionID session )";
        if (0 < eventLogs.count( session )) throw string( signature ) + " - Event log already defined for session";
        eventLogs[ session ] = Log( monitorEventsPath( CurrentProcessID() ) );
    }
    void closeEventLog( SessionID session ) {
        static const char* signature = "void closeEventLog( SessionID session )";
        if (eventLogs.count( session ) == 0) throw string( signature ) + " - No event log defined for session";
        eventLogs[ session ].close();
        eventLogs.erase( session );
    }
    LogRecord& eventRecord() {
        static const char* signature = "LogRecord& eventRecord()";
        auto session = CurrentSessionID();
        if (eventLogs.count( session ) == 0) throw string( signature ) + " - No event log defined for session";
        return eventLogs[ session ]();
    }

} // namespace AccessMonitor
