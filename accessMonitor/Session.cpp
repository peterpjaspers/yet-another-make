#include "Session.h"

#include <set>
#include <mutex>
#include <windows.h>

using namespace std;

namespace AccessMonitor {

    namespace {
    
        // Thread local storage (TLS) structure to hold session/process/thread monitoring data.
        // TLS data is used to avoid mutual exclusion when accessing this data.
        struct MonitorData {
            SessionID       session;
            std::filesystem::path directory;
            ProcessID       process;
            ThreadID        thread;
            LogFile*        eventLog;
            LogFile*        debugLog;
            MonitorGuard    fileGuard;
            MonitorGuard    threadAndProcessGuard;
        };

        unsigned int tlsSessionIndex = -1;
        mutex sessionMutex;
        set<SessionID> sessions;
        set<SessionID> freeSessions;
        SessionID nextSession = 1;

    }

    SessionID CurrentSessionID() {
        static const char* signature = "SessionID CurrentSessionID()";
        auto data = static_cast<const MonitorData*>(TlsGetValue(tlsSessionIndex));
        if (data == nullptr) throw string(signature) + " - Thread not active on a session!";
        return data->session;
    }
    std::filesystem::path const& CurrentSessionDirectory() {
        static const char* signature = "SessionID CurrentSessionDirectory()";
        auto data = static_cast<const MonitorData*>(TlsGetValue(tlsSessionIndex));
        if (data == nullptr) throw string(signature) + " - Thread not active on a session!";
        return data->directory;
    }
    SessionID CreateSession(std::filesystem::path const& directory) {
        static const char* signature = "SessionID CreateSession()";
        auto data = static_cast<MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if (data != nullptr) throw string( signature ) + " - Thread already active on a session!";
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
    }
    void CreateRemoteSession( std::filesystem::path const& sessionDirectory, SessionID session, ProcessID process, ThreadID thread ) {
        static const char* signature = "void CreateRemoteSession( SessionID session, ProcessID process, ThreadID thread )";
        throw string( signature ) + " - Invalid call!";
    }
    void RemoveSession() {
        static const char* signature = "void RemoveSession()";
        auto data = static_cast<MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if (data == nullptr) throw string( signature ) + " - Thread not active on a session!";
        const lock_guard<mutex> lock( sessionMutex );
        SessionID freeSession = data->session;
        sessions.erase( freeSession );
        freeSessions.insert( freeSession );
        LocalFree( static_cast<HLOCAL>( data ) );
        TlsSetValue( tlsSessionIndex, nullptr );
        if (sessions.size() == 0) {
            TlsFree( tlsSessionIndex );
            tlsSessionIndex = (DWORD)-1;
        }
    }
    void AddThreadToSession( SessionID session, std::filesystem::path const& directory, LogFile* eventLog, LogFile* debugLog ) {
        auto data = static_cast<MonitorData*>( LocalAlloc(LPTR , sizeof( MonitorData ) ) );
        data->session = session;
        data->directory = directory;
        data->process = CurrentProcessID();
        data->thread = CurrentThreadID();
        data->eventLog = eventLog;
        data->debugLog = debugLog;
        TlsSetValue( tlsSessionIndex, data );
    }
    void RemoveThreadFromSession() {
        auto data = static_cast<MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if (data != nullptr) LocalFree( static_cast<HLOCAL>( data ) );
        TlsSetValue( tlsSessionIndex, nullptr );
    }
    bool SessionDefined() {
        auto data = static_cast<const MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if (data == nullptr) return false;
        return true;
    }

    void SessionEventLog( LogFile* log ) {
        static const char* signature = "void SessionEventLog( void* log )";
        auto data = static_cast<const MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if (data == nullptr) throw string( signature ) + " - Thread not active on a session!";
        const_cast<MonitorData*>(data)->eventLog = log;
    }
    LogFile* SessionEventLog() {
        auto data = static_cast<const MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if (data == nullptr) return nullptr;
        return data->eventLog;
    }
    void SessionEventLogClose() {
        auto data = static_cast<const MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if ((data != nullptr) && (data->eventLog != nullptr)) data->eventLog->close();
    }

    void SessionDebugLog( LogFile* log ) {
        static const char* signature = "void SessionDebugLog( void* log )";
        auto data = static_cast<const MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if (data == nullptr) throw string( signature ) + " - Thread not active on a session!";
        const_cast<MonitorData*>(data)->debugLog = log;
    }
    LogFile* SessionDebugLog() {
        auto data = static_cast<const MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if (data == nullptr) return nullptr;
        return data->debugLog;
    }
    void SessionDebugLogClose() {
        auto data = static_cast<const MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if ((data != nullptr) && (data->debugLog != nullptr)) data->debugLog->close();
    }

    MonitorGuard* SessionFileGuard() {
        auto data = static_cast<MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if (data == nullptr) return nullptr;
        return &data->fileGuard;
    }

    MonitorGuard* SessionThreadAndProcessGuard() {
        auto data = static_cast<MonitorData*>( TlsGetValue( tlsSessionIndex ) );
        if (data == nullptr) return nullptr;
        return &data->threadAndProcessGuard;
    }

} // namespace AccessMonitor