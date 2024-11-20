#ifndef ACCESS_MONITOR_SESSION_H
#define ACCESS_MONITOR_SESSION_H

#include "Process.h"
#include "LogFile.h"

#include <filesystem>

namespace AccessMonitor {

    struct MonitorAccess {
        unsigned long monitorCount;
        unsigned long errorCode;
        inline MonitorAccess() : monitorCount( 0 ), errorCode( 0 ) {};
    };

    // Context in which a session operates.
    struct SessionContext {
        std::filesystem::path   directory;
        SessionID               session;
        LogAspects              aspects;
        SessionContext() : directory( "" ), session( 0 ), aspects( 0 ) {}
        SessionContext( const std::filesystem::path dir, const SessionID sid, const LogAspects dasp ) :
            session( sid ), directory( dir ), aspects( dasp ) {}
    };

    class Session {
        SessionContext  context;
        LogFile*        events;
        LogFile*        debug;
    public:
        Session() = delete;
        // Create a new session in the current process.
        // The thread creating the session is added to the session.
        Session( const std::filesystem::path& directory, const LogAspects aspects = 0 );
        // Create a session from an existing session.
        // Typically used to extend the session in which a process was spawned to the spawned process.
        Session( const SessionContext& context );
        // Remove session from current thread
        ~Session();
        // Release session ID for re-use.
        // Called after collecting session associated results when a session is terminated.
        static void release( SessionID id );
        // Return session for current thread.
        static Session* current();
        // Return number of currently active sessions.
        static int count();
        // Return ID associated with this session.
        SessionID id() const;
        // Return directory for session data storage associated with current thread.
        const std::filesystem::path& directory() const;
        // Add current thread to session.
        void addThread() const;
        // Remove current thread from session.
        void removeThread() const;
        // Set and/or close session event log.
        // If the session has an event log, it is closed (typically by with a nnullptr argument).
        void eventLog( LogFile* file );
        // Return session event log.
        LogFile* eventLog() const;
        // Close the session event log.
        void eventClose();
        // Set and/or close session debug log.
        // If the session has an dbug log, it is closed (typically by with a nnullptr argument).
        void debugLog( LogFile* file );
        // Return session debug log.
        LogFile* debugLog() const;
        // Close the session debug log.
        void debugClose();
        // Return monitor access for file access in this session.
        static MonitorAccess* fileAccess();
        // Return monitor access for process access in this session.
        static MonitorAccess* processAccess();
        // Record session context for a (spawned/remote) process.
        // The recorded session context may be retrieved in the (remote) process with a call to retrieveContext.
        void* recordContext( const ProcessID process ) const;
        // Release session context record, freeing memory resources.
        // Session context record should released after being retrieved in the (remote) process.
        static void releaseContext( void* context );
        // Retreive session context for/in a (spawned/remote) process recorded by a call to recordContext.
        // The retrieved context may be used to extend a session to the (remote) process.
        static const SessionContext retrieveContext( const ProcessID process = CurrentProcessID() );

    };

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_SESSION_H