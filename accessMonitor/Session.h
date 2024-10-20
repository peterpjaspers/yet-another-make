#ifndef ACCESS_MONITOR_SESSION_H
#define ACCESS_MONITOR_SESSION_H

#include "Process.h"
#include "LogFile.h"
#include "Patch.h"

#include <filesystem>

namespace AccessMonitor {

    // Session ID indicating that a new session is to be created (in the main process).
    const SessionID CreateNewSession = (SessionID)-1;


    // Get SessionID of session associated with current thread
    SessionID CurrentSessionID();
    // Get directory for session data storage associated with current thread.
    const std::filesystem::path& CurrentSessionDirectory();
    // Create a new session on current thread or open an existing session (in a remote process).
    // Store session data in given directory.
    // Creates a new session with CreateNewSession as session argument.
    SessionID CreateSession( const std::filesystem::path& directory, const SessionID session = CreateNewSession );
    // Remove session from current thread
    void RemoveSession();
    // Add current thread to session
    void AddThreadToSession( const std::filesystem::path& directory, const SessionID session, LogFile* eventLog = nullptr, LogFile* debugLog = nullptr );
    // Remove current thread from session 
    void RemoveThreadFromSession();

    // Return number of active sessions
    int SessionCount();

    // Set/get event log for current session.
    void SessionEventLog( LogFile* log );
    LogFile* SessionEventLog();
    void SessionEventLogClose();
    // Set/get debug log for current session.
    void SessionDebugLog( LogFile* log );
    LogFile* SessionDebugLog();
    void SessionDebugLogClose();

    struct MonitorAccess {
        int monitorCount;
        unsigned long errorCode;
        inline MonitorAccess() : monitorCount( 0 ), errorCode( 0 ) {};
    };

    // Return file monitoring access data.
    MonitorAccess* SessionFileAccess();
    // Return thread and process monitoring access data.
    MonitorAccess* SessionThreadAndProcessAccess();

    // Record session conetxt for a (remote) process.
    // Recorded session context should released after being retrieved in the remote process.
    void* recordSessionContext( const ProcessID process, const SessionID session, const LogAspects apspects, const std::filesystem::path& dir );
    // Release recorded session context, freeing memory resources.
    void releaseSessionContext( void* map );
    // Retrieve session context in a (remote) process.
    void retrieveSessionContext( const ProcessID process, SessionID& session, LogAspects& aspects, std::filesystem::path& dir );


} // namespace AccessMonitor

#endif // ACCESS_MONITOR_SESSION_H