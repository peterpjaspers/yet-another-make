#include "Session.h"

#include <set>
#include <mutex>
#include <windows.h>

using namespace std;
using namespace filesystem;

// ToDo: Refactor code to make use of an object class Session.

namespace AccessMonitor {

    namespace {
    
        // Context in which monitor data is gathered for a session.
        // In the main process there may be multiple sessions with corresponding event and debug logs.
        // In a remote process there is only a single session and consequently only a single event and debug log.
        // The monitor access guards are unique per thread.
        // The monitor context is accessed via Thread Local Storage (TLS) to avoid synchronization.
        struct SessionContext {
            const path      directory;
            const SessionID session;
            const ProcessID process;
            const ThreadID  thread;
            LogFile*        eventLog;
            LogFile*        debugLog;
            MonitorAccess   fileGuard;
            MonitorAccess   threadAndProcessGuard;
            SessionContext( const path dir, const SessionID sid, const ProcessID pid, const ThreadID tid ) :
                session( sid ), directory( dir ), process( pid ), thread( tid ) {}
        };

        unsigned int tlsSessionIndex = -1;
        mutex sessionMutex;
        set<SessionID> sessions;
        set<SessionID> freeSessions;
        SessionID nextSession = 1;

        inline string sessionInfoMapName( const ProcessID process ) {
            stringstream unique;
            unique << "RemoteProcessSessionData" << "_" << process;
            return unique.str();
        }

        // Session context passeed to spawned process to enable creating (opening) a session in a remote process.
        // See recordSessionContext and retrieveSessionContext..
        struct SessionConextData {
            SessionID   session;                // The session in which the process was spawned
            LogAspects  aspects;           // The debugging aspects to be applied in the spawned process
            char        directory[ MAX_PATH ];  // The directory in which monitor data is stored
        };

        SessionContext* remoteSessionContext = nullptr;

    }

    // Retrieve monitor context for current thread.
    // Throws exception with given signature if thread is not active on a session.
    SessionContext* sessionContext( const char* signature ) {
        auto context = static_cast<SessionContext*>(TlsGetValue(tlsSessionIndex));
        if (context == nullptr) {
            if (remoteSessionContext != nullptr) {
                // Executing in a thread that was not explicitly created in a session (i.e., main thread).
                AddThreadToSession( remoteSessionContext->directory, remoteSessionContext->session, remoteSessionContext->eventLog, remoteSessionContext->debugLog );
                return sessionContext( signature );
            }
            throw string(signature) + " - Thread not active on a session!";
        }
        return context;
    }

    SessionID CurrentSessionID() {
        auto context = sessionContext( "SessionID CurrentSessionID()" );
        return context->session;
    }
    const std::filesystem::path& CurrentSessionDirectory() {
        auto context = sessionContext( "const std::filesystem::path& CurrentSessionDirectory()" );
        return context->directory;
    }
    SessionID CreateSession( const std::filesystem::path& directory, const SessionID session ) {
        static const char* signature = "SessionID CreateSession( std::filesystem::path const& directory, SessionID session, ProcessID process, ThreadID thread )";
        auto context = static_cast<SessionContext*>(TlsGetValue(tlsSessionIndex));
        if (context != nullptr) throw string( signature ) + " - Thread already active on a session!";
        if (session == CreateNewSession) {
            // Create new session in root process...
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
            AddThreadToSession( directory, newSession );
            return newSession;
        } else {
            // Create session in spawned (remote) process.
            // Session was created in a parent of the spawned.
            sessions.insert( session );
            if (tlsSessionIndex == (DWORD)-1) tlsSessionIndex = TlsAlloc();
            if (tlsSessionIndex == TLS_OUT_OF_INDEXES) throw string( signature ) + " - Could not allocate thread local storage index!";
            AddThreadToSession( directory, session );
            remoteSessionContext = sessionContext( signature );
            return session;
        }
    }
    void RemoveSession() {
        static const char* signature = "void RemoveSession()";
        auto context = static_cast<SessionContext*>(TlsGetValue(tlsSessionIndex));
        if (context == nullptr) throw string( signature ) + " - Thread not active on a session!";
        const lock_guard<mutex> lock( sessionMutex );
        SessionID freeSession = context->session;
        sessions.erase( freeSession );
        freeSessions.insert( freeSession );
        LocalFree( static_cast<HLOCAL>( context ) );
        TlsSetValue( tlsSessionIndex, nullptr );
        if (SessionCount() == 0) {
            // Removing last active session...
            TlsFree( tlsSessionIndex );
            tlsSessionIndex = (DWORD)-1;
        }
    }
    int SessionCount() { return sessions.size(); }

    void AddThreadToSession( const std::filesystem::path& directory, const SessionID session, LogFile* eventLog, LogFile* debugLog ) {
        static const char* signature = "void AddThreadToSession( const std::filesystem::path& directory, const SessionID session, LogFile* eventLog, LogFile* debugLog )";
        auto context = static_cast<SessionContext*>(TlsGetValue(tlsSessionIndex));
        if (context != nullptr) throw string( signature ) + " - Thread already active on a session!";
        auto tlsData = LocalAlloc(LPTR , sizeof( SessionContext ) );
        context = new( tlsData ) SessionContext( directory, session, CurrentProcessID(), CurrentThreadID() );
        context->eventLog = eventLog;
        context->debugLog = debugLog;
        TlsSetValue( tlsSessionIndex, context );
    }
    void RemoveThreadFromSession() {
        auto context = sessionContext( "void RemoveThreadFromSession()" );
        LocalFree( static_cast<HLOCAL>( context ) );
        TlsSetValue( tlsSessionIndex, nullptr );
    }

    void SessionEventLog( LogFile* log ) {
        auto context = sessionContext( "void SessionEventLog( void* log )" );
        context->eventLog = log;
        if (remoteSessionContext != nullptr) remoteSessionContext->eventLog;
    }
    LogFile* SessionEventLog() {
        auto context = sessionContext( "LogFile* SessionEventLog()" );
        return context->eventLog;
    }
    void SessionEventLogClose() {
        auto context = sessionContext( "void SessionEventLogClose()" );
        if ((context != nullptr) && (context->eventLog != nullptr)) {
            auto log = context->eventLog;
            context->eventLog = nullptr;
            delete log;
        }
    }

    void SessionDebugLog( LogFile* log ) {
        auto context = sessionContext( "void SessionDebugLog( void* log )" );
        context->debugLog = log;
        if (remoteSessionContext != nullptr) remoteSessionContext->debugLog;
    }
    LogFile* SessionDebugLog() {
        auto context = sessionContext( "LogFile* SessionDebugLog()" );
        return context->debugLog;
    }
    void SessionDebugLogClose() {
        auto context = sessionContext( "void SessionDebugLogClose()" );
        if ((context != nullptr) && (context->debugLog != nullptr)) {
            auto log = context->debugLog;
            context->debugLog = nullptr;
            delete log;
        }
    }

    MonitorAccess* SessionFileAccess() {
        auto context = static_cast<SessionContext*>(TlsGetValue(tlsSessionIndex));
        if ((context == nullptr) && (remoteSessionContext != nullptr)) {
            AddThreadToSession( remoteSessionContext->directory, remoteSessionContext->session, remoteSessionContext->eventLog, remoteSessionContext->debugLog );
        }
        if (context != nullptr) return &context->fileGuard;
        return nullptr;
    }

    MonitorAccess* SessionThreadAndProcessAccess() {
        auto context = static_cast<SessionContext*>(TlsGetValue(tlsSessionIndex));
        if ((context == nullptr) && (remoteSessionContext != nullptr)) {
            AddThreadToSession( remoteSessionContext->directory, remoteSessionContext->session, remoteSessionContext->eventLog, remoteSessionContext->debugLog );
        }
        if (context != nullptr) return &context->threadAndProcessGuard;
        return nullptr;
    }

    // Record session ID and main thread ID of a (remote) process.
    void* recordSessionContext( const ProcessID process, const SessionID session, const LogAspects aspects, const path& dir ) {
        const char* signature( "void recordSessionContext( const path& dir, const ProcessID process, const SessionID session, ThreadID thread )" );
        auto map = CreateFileMappingA( INVALID_HANDLE_VALUE, nullptr,   PAGE_READWRITE, 0, sizeof( SessionConextData ), sessionInfoMapName( process ).c_str() );
        auto error = GetLastError();
        if (map == nullptr) throw string( signature ) + " - Could not create file mapping!";
        auto address = MapViewOfFile( map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof( SessionConextData ) );
        if (address == nullptr) throw string( signature ) + " - Could not open mapping!";
        auto context = static_cast<SessionConextData*>( address );
        context->session = session;
        context->aspects = aspects;
        const auto dirString = dir.generic_string();
        memcpy( context->directory, dirString.c_str(), dirString.size() );
        UnmapViewOfFile( address );
        return map;
    }
    void releaseSessionContext( void* map ) { CloseHandle( map ); }
    // Retrieve session ID and main thread ID of a (remote) process
    void retrieveSessionContext( const ProcessID process, SessionID& session, LogAspects& aspects, path& dir ) {
        const char* signature( "void retrieveSessionContext( const path& dir, const ProcessID process, SessionID& session, ThreadID& thread )" );
        auto map = OpenFileMappingA( FILE_MAP_ALL_ACCESS, false, sessionInfoMapName( process ).c_str() );
        if (map == nullptr) throw string( signature ) + " - Could not create file mapping!";
        void* address = MapViewOfFile( map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof( SessionConextData ) );
        if (address == nullptr) throw string( signature ) + " - Could not open mapping!";
        auto context = static_cast<const SessionConextData*>( address );
        session = context->session;
        aspects = context->aspects;
        dir = context->directory;
        UnmapViewOfFile( address );
        CloseHandle( map );
    }

} // namespace AccessMonitor