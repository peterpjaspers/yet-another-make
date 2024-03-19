#include "Process.h"

#include <sstream>
#include <map>
#include <set>
#include <mutex>

using namespace std;

// Encapsulate OS specific process control
// ToDo: Implenent for Linux and MacOS, current implementation is Windows only...

namespace AccessMonitor {

    namespace {

        mutex sessionMutex;
        map<SessionID,set<ThreadID>> sessionToThreads;
        map<ThreadID,SessionID> threadToSession;
        map<ThreadID,SessionState> sessionStates;
        SessionID nextSession = 1;

        string uniqueEventName( const string& tag, const SessionID session, const ProcessID process ) {
            stringstream name;
            name << "Global\\" << tag << "_Event_" << session << "_" << hex << process;
            return name.str();
        }

    }

    SessionID CurrentSessionID() { return ThreadSessionID( CurrentThreadID() ); }
    SessionID ThreadSessionID( ThreadID thread ) {
        static const char* signature = "SessionID ThreadSessionID( ThreadID thread )";
        const lock_guard<mutex> lock( sessionMutex ); 
        if (threadToSession.count( thread ) == 0) throw string( signature ) + " - Thread not active on a session!";
        return threadToSession[ thread ];
    }
    size_t SessionCount() {
        const lock_guard<mutex> lock( sessionMutex ); 
        return sessionToThreads.size();
    }
    SessionID CreateSession() {
        static const char* signature = "SessionID CreateSession()";
        ThreadID thread = CurrentThreadID();
        const lock_guard<mutex> lock( sessionMutex ); 
        if (0 < threadToSession.count( thread )) throw string( signature ) + " - Thread already active on a session!";
        SessionID session = nextSession++;
        threadToSession[ thread ] = session;
        sessionToThreads[ session ].insert( thread );
        sessionStates[ session ] = SessionActive;
        return session;
    }
    void RemoveSession() {
        static const char* signature = "void RemoveSession()";
        ThreadID thread = CurrentThreadID();
        const lock_guard<mutex> lock( sessionMutex ); 
        if (threadToSession.count( thread ) == 0) throw string( signature ) + " - Thread not active on a session!";
        auto session = threadToSession[ thread ];
        sessionToThreads[ session ].erase( thread );
        for (auto thread : sessionToThreads[ session ]) threadToSession.erase( thread );
        sessionToThreads.erase( session );
        sessionStates.erase( session );
        if (threadToSession.size() == 0) nextSession = 1;
    }
    void AddSessionThread( ThreadID thread, SessionID session ) {
        const lock_guard<mutex> lock( sessionMutex ); 
        threadToSession[ thread ] = session;
        sessionToThreads[ session ].insert( thread );
    }
    void RemoveSessionThread( ThreadID thread ) {
        const lock_guard<mutex> lock( sessionMutex );
        SessionID session = threadToSession[ thread ];
        sessionToThreads[ session ].erase( thread );
        threadToSession.erase( thread );
    }
    void SetSessionState( const SessionState state ) {
        static const char* signature = "void SessionActive( const bool state )";
        auto thread = CurrentThreadID();
        const lock_guard<mutex> lock( sessionMutex ); 
        if (threadToSession.count( thread ) == 0) throw string( signature ) + " - Thread not active on a session!";
        sessionStates[ threadToSession[ thread ] ] = state;
    }
    SessionState GetSessionState() {
        auto thread = CurrentThreadID();
        const lock_guard<mutex> lock( sessionMutex ); 
        if (threadToSession.count( thread ) == 0) return SessionNone;
        return sessionStates[ threadToSession[ thread ] ];
    }
    bool SessionDefined() { return (GetSessionState() == SessionActive); }

    ProcessID CurrentProcessID() { return static_cast<ProcessID>( GetCurrentProcessId() ); }
    ProcessID GetProcessID( DWORD id ) { return static_cast<ProcessID>( id ); }

    ThreadID CurrentThreadID() { return static_cast<ThreadID>( GetCurrentThreadId() ); }
    ThreadID GetThreadID( DWORD id ) { return static_cast<ThreadID>( id ); }

    EventID AccessEvent( const string& tag, const SessionID session, const ProcessID process ) {
        HANDLE handle = CreateEventA( nullptr, false, false, uniqueEventName( tag, session, process ).c_str() );
        return static_cast<EventID>( handle );
    }
    void ReleaseEvent( EventID event ) { CloseHandle( event ); }

    bool EventWait( EventID event, unsigned long milliseconds ) {
        DWORD status = WaitForSingleObject( event, milliseconds );
        if (status == WAIT_OBJECT_0) return true;
        return false;
    }
    bool EventWait( const std::string& tag, const SessionID session, const ProcessID process, unsigned long milliseconds ) {
        auto event = AccessEvent( tag, session, process );
        auto signaled = EventWait( event, milliseconds );
        ReleaseEvent( event );
        return signaled;
    }

    void EventSignal( EventID event ) { SetEvent( event ); }
    void EventSignal( const std::string& tag, const SessionID session, const ProcessID process ) {
        auto event = AccessEvent( tag, session, process );
        EventSignal( event );
        ReleaseEvent( event );
    }

} // namespace AccessMonitor
