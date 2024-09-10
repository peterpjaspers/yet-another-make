#ifndef ACCESS_MONITOR_SESSION_H
#define ACCESS_MONITOR_SESSION_H

#include "Process.h"
#include "LogFile.h"
#include "Patch.h"

#include <filesystem>

namespace AccessMonitor {

    // Get SessionID of session associated with current thread
    SessionID CurrentSessionID();
    // Get directory for session data storage associated with current thread.
    std::filesystem::path const& CurrentSessionDirectory();
    // Create a new session on current thread.
    // Store session data in given directory.
    // If 
    SessionID CreateSession( std::filesystem::path const& directory, SessionID session = CreateNewSession, ProcessID process = CurrentProcessID(), ThreadID thread = CurrentThreadID() );
    // Remove session from current thread
    void RemoveSession();
    // Add current thread to session
    void AddThreadToSession( SessionID session, std::filesystem::path const& directory, LogFile* eventLog = nullptr, LogFile* debugLog = nullptr );
    // Remove current thread from session 
    void RemoveThreadFromSession();

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

    // Initialize session if not yet initialized.
    // The first thread to call this function will perform the actual inialization.
    // Other threads will be blocked until initialization is completed.
    // Once initialized, the overhead is a test on a boolean value.
    void SessionInitialize();

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_SESSION_H