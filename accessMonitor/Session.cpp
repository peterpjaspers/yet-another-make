#include "Session.h"

#include <set>
#include <mutex>
#include <windows.h>

using namespace std;
using namespace filesystem;

namespace AccessMonitor {

    namespace {
    
        // Context in which monitor data is gathered.
        struct MonitorContext {
            SessionID       session;
            path            directory;
            ProcessID       process;
            ThreadID        thread;
            LogFile*        eventLog;
            LogFile*        debugLog;
            MonitorAccess   fileGuard;
            MonitorAccess   threadAndProcessGuard;
        };

        unsigned int tlsSessionIndex = -1;
        mutex sessionMutex;
        set<SessionID> sessions;
        set<SessionID> freeSessions;
        SessionID nextSession = 1;
        // Monitoring context for session in a remote process.
        // In a remote process there is only a single session and consequently monitor context.
        // In the main process there is a monitor context for each session.
        // If the (remote process) monitor conext is null we are executing in the main process
        // and the monitor context is accessed via thread local storage (TLS) at tlsSessionIntex.
        // TLS data is used to avoid mutual exclusion when accessing the monitor context.
        MonitorContext* remoteContext = nullptr;

    }

    // Retrieve monitor context for current (possibly remote) session.
    // Throws exception with given signature if thread is not active on a session.
    inline MonitorContext* sessionMonitorContext( const char* signature ) {
        auto context = remoteContext;
        if (context == nullptr) context = static_cast<MonitorContext*>(TlsGetValue(tlsSessionIndex));
        if (context == nullptr) throw string(signature) + " - Thread not active on a session!";
        return context;
    }

    SessionID CurrentSessionID() {
        auto context = sessionMonitorContext( "SessionID CurrentSessionID()" );
        return context->session;
    }
    std::filesystem::path const& CurrentSessionDirectory() {
        auto context = sessionMonitorContext( "std::filesystem::path const& CurrentSessionDirectory()" );
        return context->directory;
    }
    SessionID CreateSession( std::filesystem::path const& directory, SessionID session, ProcessID process, ThreadID thread ) {
        static const char* signature = "SessionID CreateSession( std::filesystem::path const& directory, SessionID session, ProcessID process, ThreadID thread )";
        if (session == CreateNewSession) {
            // Create new session in main process
            if (remoteContext != nullptr) throw string(signature) + " - Creating new session in exsting remote session!";
            auto context = static_cast<MonitorContext*>(TlsGetValue(tlsSessionIndex));
            if (context != nullptr) throw string( signature ) + " - Thread already active on a session!";
            const lock_guard<mutex> lock( sessionMutex );
            if (tlsSessionIndex == (DWORD)-1) tlsSessionIndex = TlsAlloc();
            if (tlsSessionIndex == TLS_OUT_OF_INDEXES) throw string( signature ) + " - Could not allocate thread local storage index!";
            SessionID newSession;
            if (0 < freeSessions.size()) {
                newSession = *freeSessions.begin();
                freeSessions.erase( freeSessions.begin() );
            } else {
                newSession = nextSession++;
            }
            sessions.insert( newSession );
            AddThreadToSession( newSession, directory );
            return newSession;
        } else {
            // Create session in remote process
            auto context = remoteContext;
            if (context != nullptr) throw string( signature ) + " - Remote session already active!";
            const lock_guard<mutex> lock( sessionMutex );
            context = static_cast<MonitorContext*>( LocalAlloc( LPTR, sizeof( MonitorContext ) ) );
            context->session = session;
            context->directory = directory;
            context->process = process;
            context->thread = thread;
            remoteContext = context;
            return session;
        }
    }
    void RemoveSession() {
        static const char* signature = "void RemoveSession()";
        if (remoteContext == nullptr) {
            // Removing session in main process
            auto context = static_cast<MonitorContext*>(TlsGetValue(tlsSessionIndex));
            if (context == nullptr) throw string( signature ) + " - Thread not active on a session!";
            const lock_guard<mutex> lock( sessionMutex );
            SessionID freeSession = context->session;
            sessions.erase( freeSession );
            freeSessions.insert( freeSession );
            LocalFree( static_cast<HLOCAL>( context ) );
            TlsSetValue( tlsSessionIndex, nullptr );
            if (sessions.size() == 0) {
                // Removing last session in main process
                TlsFree( tlsSessionIndex );
                tlsSessionIndex = (DWORD)-1;
            }
        } else {
            // Removing session in remote process
            const lock_guard<mutex> lock( sessionMutex );
            LocalFree( static_cast<HLOCAL>( const_cast<MonitorContext*>( remoteContext ) ) );
            remoteContext = nullptr;
        }
    }
    void AddThreadToSession( SessionID session, std::filesystem::path const& directory, LogFile* eventLog, LogFile* debugLog ) {
        static const char* signature = "void AddThreadToSession( SessionID session, std::filesystem::path const& directory, LogFile* eventLog, LogFile* debugLog )";
        if (remoteContext == nullptr) {
            auto context = static_cast<MonitorContext*>(TlsGetValue(tlsSessionIndex));
            if (context != nullptr) throw string( signature ) + " - Thread already active on a session!";
            context = static_cast<MonitorContext*>( LocalAlloc(LPTR , sizeof( MonitorContext ) ) );
            context->session = session;
            context->directory = directory;
            context->process = CurrentProcessID();
            context->thread = CurrentThreadID();
            context->eventLog = eventLog;
            context->debugLog = debugLog;
            TlsSetValue( tlsSessionIndex, context );
        }
    }
    void RemoveThreadFromSession() {
        auto context = sessionMonitorContext( "void RemoveThreadFromSession()" );
        if (context != remoteContext) {
            LocalFree( static_cast<HLOCAL>( context ) );
            TlsSetValue( tlsSessionIndex, nullptr );
        }
    }

    void SessionEventLog( LogFile* log ) {
        auto context = sessionMonitorContext( "void SessionEventLog( void* log )" );
        context->eventLog = log;
    }
    LogFile* SessionEventLog() {
        auto context = sessionMonitorContext( "LogFile* SessionEventLog()" );
        return context->eventLog;
    }
    void SessionEventLogClose() {
        auto context = sessionMonitorContext( "void SessionEventLogClose()" );
        if ((context != nullptr) && (context->eventLog != nullptr)) context->eventLog->close();
    }

    void SessionDebugLog( LogFile* log ) {
        auto context = sessionMonitorContext( "void SessionDebugLog( void* log )" );
        context->debugLog = log;
    }
    LogFile* SessionDebugLog() {
        auto context = sessionMonitorContext( "LogFile* SessionDebugLog()" );
        return context->debugLog;
    }
    void SessionDebugLogClose() {
        auto context = sessionMonitorContext( " void SessionDebugLogClose()" );
        if ((context != nullptr) && (context->debugLog != nullptr)) context->debugLog->close();
    }

    void SessionInitialize() {
        static bool initialized = false;
        static std::mutex initializing;
        if (!initialized) {
            const std::lock_guard<std::mutex> lock( initializing );
            // Re-test flag to avoid multiple initialization...
            if (!initialized) {
                // Do actual initialization...
                initialized = true;
            }
        }
    }

    MonitorAccess* SessionFileAccess() {
        auto context = static_cast<MonitorContext*>( TlsGetValue( tlsSessionIndex ) );
        if (context == nullptr) return nullptr;
        return &context->fileGuard;
    }

    MonitorAccess* SessionThreadAndProcessAccess() {
        auto context = static_cast<MonitorContext*>( TlsGetValue( tlsSessionIndex ) );
        if (context == nullptr) return nullptr;
        return &context->threadAndProcessGuard;
    }

} // namespace AccessMonitor