#include "Session.h"

#include <mutex>
#include <windows.h>

using namespace std;

namespace AccessMonitor {

    namespace {
    
        // Thread local storage (TLS) structure to hold session/process/thread monitoring data.
        // TLS data is used to avoid mutual exclusion when accessing this data.
        struct MonitorContext {
            SessionID       session;
            std::filesystem::path directory;
            ProcessID       process;
            ThreadID        thread;
            LogFile*        eventLog;
            LogFile*        debugLog;
            MonitorAccess    fileGuard;
            MonitorAccess    threadAndProcessGuard;
        };

        mutex sessionMutex;
        const MonitorContext* sessionData = nullptr;

    }

    SessionID CurrentSessionID() { 
        static const char* signature = "SessionID CurrentSessionID()";
        if (sessionData == nullptr) throw string( signature ) + " - No session active!";
        return sessionData->session;
    }
    std::filesystem::path const& CurrentSessionDirectory() {
        static const char* signature = "SessionID CurrentSessionDirectory()";
        if (sessionData == nullptr) throw string(signature) + " - No session active!";
        return sessionData->directory;
    }
    SessionID CreateSession( std::filesystem::path const& directory ) {
        static const char* signature = "SessionID CreateSession()";
        throw string( signature ) + " - Invalid call!";
    }
    void CreateRemoteSession( std::filesystem::path const& directory, SessionID session, ProcessID process, ThreadID thread ) {
        static const char* signature = "void CreateRemoteSession( SessionID session, ProcessID process, ThreadID thread )";
        if (sessionData != nullptr) throw string( signature ) + " - Remote session already created!";
        const lock_guard<mutex> lock( sessionMutex );
        auto data = static_cast<MonitorContext*>( LocalAlloc( LPTR, sizeof( MonitorContext ) ) );
        data->session = session;
        data->directory = directory;
        data->process = process;
        data->thread = thread;
        sessionData = data;
    }
    void RemoveSession() {
        static const char* signature = "void RemoveSession()";
        const lock_guard<mutex> lock( sessionMutex );
        if (sessionData == nullptr) throw string( signature ) + " - No active session!";
        LocalFree( static_cast<HLOCAL>( const_cast<MonitorContext*>( sessionData ) ) );
        sessionData = nullptr;
    }
    void AddThreadToSession( SessionID session, std::filesystem::path const& directory, LogFile* eventLog, LogFile* debugLog ) {}
    void RemoveThreadFromSession() {}

    void SessionEventLog( LogFile* log ) {
        static const char* signature = "void SessionEventLog( void* log )";
        if (sessionData == nullptr) throw string( signature ) + " - No active session!";
        const_cast<MonitorContext*>(sessionData)->eventLog = log;
    }
    LogFile* SessionEventLog() {
        if (sessionData == nullptr) return nullptr;
        return sessionData->eventLog;
    }
    void SessionEventLogClose() {
        if ((sessionData != nullptr) && (sessionData->eventLog != nullptr)) sessionData->eventLog->close();
    }

    void SessionDebugLog( LogFile* log ) {
        static const char* signature = "void SessionDebugLog( void* log )";
        if (sessionData == nullptr) throw string( signature ) + " - No active session!";
        const_cast<MonitorContext*>(sessionData)->debugLog = log;
    }
    LogFile* SessionDebugLog() {
        if (sessionData == nullptr) return nullptr;
        return sessionData->debugLog;
    }
    void SessionDebugLogClose() {
        if ((sessionData != nullptr) && (sessionData->debugLog != nullptr)) sessionData->debugLog->close();
    }

    MonitorAccess* SessionFileAccess() {
        if (sessionData == nullptr) return nullptr;
        return const_cast<MonitorAccess*>( &sessionData->fileGuard );
    }

    MonitorAccess* SessionThreadAndProcessAccess() {
        if (sessionData == nullptr) return nullptr;
        return const_cast<MonitorAccess*>( &sessionData->threadAndProcessGuard );
    }

} // namespace AccessMonitor