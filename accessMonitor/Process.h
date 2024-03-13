#ifndef ACCESS_MONITOR_PROCESS_H
#define ACCESS_MONITOR_PROCESS_H

#include <windows.h>
#include <string>

namespace AccessMonitor {

    typedef unsigned long SessionID;
    typedef unsigned long ProcessID;
    typedef unsigned long ThreadID;
    typedef void* EventID;

    enum SessionState {
        SessionNone         = 0,    // Session does not exist
        SessionActive       = 1,    // Session is actively recording events
        SessionUpdating     = 2,    // Session administration is being updated
    };

    // Get SessionID of session associated with the current thrtead
    SessionID CurrentSessionID();
    // Get SessionID of session associated with a thread
    SessionID ThreadSessionID( ThreadID thread );
    // Get number of active sessions
    size_t SessionCount();
    // Create a new session on the current thread
    SessionID CreateSession();
    // Remove a session from the current thread
    void RemoveSession();
    // Add (additional) thread to session administration
    void AddSessionThread( ThreadID thread, SessionID session );
    // Remove (additional) thread from session administration
    void RemoveSessionThread( ThreadID thread );
    // Set/get session state:
    //    SessionNone, SessionActive or SessionUpdating
    void SetSessionState( const SessionState state );
    SessionState GetSessionState();
    // Test if session is defined on current thread
    bool SessionDefined();

    // Get ProcessID of current process
    ProcessID CurrentProcessID();
    // Convert OS specific ID of process to ProcessID
    ProcessID GetProcessID( DWORD id );
    
    // Get ThreadID of current thread
    ThreadID CurrentThreadID();
    // Convert OS specific ID of thread to ThreadID
    ThreadID GetThreadID( DWORD id );
    
    // Request access to (unique) named event for a process.
    // The event is created if it does not already exist.
    EventID AccessEvent( const std::string& tag, const SessionID session, const ProcessID process );
    // Release access to (unique) named event.
    // The event is destroyed when it is no longer accessed.
    void ReleaseEvent( EventID event );

    // Wait for an event with an otional time-out
    bool EventWait( EventID event, unsigned long milliseconds = ULONG_MAX );
    // Wait for a named event
    // Unless defined elsewhere, event lifetime of named event is restricted to duration of EventWait unless.
    bool EventWait( const std::string& tag, const SessionID session, const ProcessID process, unsigned long milliseconds = ULONG_MAX );
    // Signal an event
    void EventSignal( EventID event );
    // Signal for a named event
    // Unless defined elsewhere, event lifetime of named event is restricted to duration of EventWait unless.
    void EventSignal( const std::string& tag, const SessionID session, const ProcessID process );

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_PROCESS_H
