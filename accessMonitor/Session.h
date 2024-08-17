#ifndef ACCESS_MONITOR_SESSION_H
#define ACCESS_MONITOR_SESSION_H

#include "Process.h"
#include "LogFile.h"

#include <filesystem>

namespace AccessMonitor {

    // Get SessionID of session associated with the current thread
    SessionID CurrentSessionID();
    // Get directory for temporary data storage of session associated with the
    // current thread.
    std::filesystem::path const& CurrentSessionDirectory();
    // Create a new session on the current thread.
    // Store session temporary data in given directory.
    SessionID CreateSession(std::filesystem::path const& directory);
    // Create existing session in remote process
    void CreateRemoteSession(std::filesystem::path const& directory, SessionID session, ProcessID process, ThreadID thread );
    // Remove a session from the current thread
    void RemoveSession();
    // Add current thread to session
    void AddThreadToSession( SessionID session, std::filesystem::path const& directory, LogFile* eventLog = nullptr, LogFile* debugLog = nullptr );
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

    struct MonitorGuard {
        bool monitoring;
        unsigned long errorCode;
        inline MonitorGuard() : monitoring( false ), errorCode( 0 ) {};
    };

    // Access file monitoring guard data.
    MonitorGuard* SessionFileGuard();
    // Access thread and process monitoring guard data.
    MonitorGuard* SessionThreadAndProcessGuard();


} // namespace AccessMonitor

#endif // ACCESS_MONITOR_SESSION_H