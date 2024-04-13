#ifndef ACCESS_MONITOR_SESSION_H
#define ACCESS_MONITOR_SESSION_H

#include "Process.h"
#include "LogFile.h"

namespace AccessMonitor {

    // Get SessionID of session associated with the current thread
    SessionID CurrentSessionID();
    // Create a new session on the current thread
    SessionID CreateSession();
    // Create existing session in remote process
    void CreateRemoteSession( SessionID session, ProcessID process, ThreadID thread );
    // Remove a session from the current thread
    void RemoveSession();
    // Add current thread to session
    void AddThreadToSession( SessionID session, LogFile* eventLog = nullptr, LogFile* debugLog = nullptr );
    // Remove current thread from session 
    void RemoveThreadFromSession();
    // Test if session is defined on current thread
    bool SessionDefined();

    // Set/get event log/record for current session thread.
    void SessionEventLog( LogFile* log );
    LogFile* SessionEventLog();
    void SessionEventLogClose();
    // Set/get debug log/record for current session thread.
    void SessionDebugLog( LogFile* log );
    LogFile* SessionDebugLog();
    void SessionDebugLogClose();

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_SESSION_H