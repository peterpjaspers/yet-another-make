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
            directory( dir ), session( sid ), aspects( dasp ) {}
    };

    class Session {
        SessionContext  context;
        LogFile*        events;     // Events log file for all events from all threads in this session
        LogFile*        debug;      // Debug log file when access monitor logging is enabled
    public:
        static const SessionID MaxSessionID = 100;
        inline Session() : context( { "", 0, 0 } ), events( nullptr ), debug( nullptr ) {};
        // Start a new session in the current process.
        // The thread creating the session is added to the session.
        static Session* start( const std::filesystem::path& directory, const LogAspects aspects = 0 );
        // Start a session in a remote process of an existing session.
        // Typically used to extend the session in which a process was spawned to the spawned process.
        static Session* start( const SessionContext& context );
        // Terminate session. Closes event log and (optionally) debug log.
        void terminate();
        // Check if a session is terminated.
        bool terminated();
        // Check if a session is free.
        bool free();
        // Stop a session.
        void stop();
        // Return session associated with a session ID.
        static Session* session( SessionID id );
        // Return session for current thread.
        static Session* current();
        // Return number of currently active sessions.
        static int count();
        // Return directory for monitored event data storage for this session.
        inline const std::filesystem::path& directory() const { return( context.directory ); }
        // Return ID associated with this session.
        inline SessionID id() const { return( context.session ); }
        // Return debug log aspects effective in this session.
        inline LogAspects aspects() const { return( context.aspects ); }
        // Add current thread to session.
        void addThread() const;
        // Remove current thread from session.
        void removeThread() const;
        // Set and/or close session event log.
        // If the session has an event log, it is first closed.
        void eventLog( LogFile* file );
        // Return session event log.
        LogFile* eventLog() const;
        // Set and/or close session debug log.
        // If the session has a debug log, it is first closed.
        void debugLog( LogFile* file );
        // Return session debug log.
        LogFile* debugLog() const;
        // Return monitor access for logging in this session.
        static MonitorAccess* monitorAccess();
        // Record session context for a (spawned/remote) process.
        // The recorded session context may be retrieved in the (remote) process with a call to retrieveContext.
        void* recordContext( const ProcessID process ) const;
        // Release session context record, freeing memory resources.
        // Session context record should released after being retrieved in the (remote) process.
        static void releaseContext( void* context );
        // Retreive session context for/in a (spawned/remote) process recorded by a call to recordContext.
        // The retrieved context may be used to extend a session to the (remote) process.
        static const SessionContext retrieveContext( const ProcessID process = CurrentProcessID() );
        static SessionID initialize();
    private:
        void _terminate();
    };

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_SESSION_H