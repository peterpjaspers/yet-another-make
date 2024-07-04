#ifndef ACCESS_MONITOR_PROCESS_H
#define ACCESS_MONITOR_PROCESS_H

#include "LogFile.h"

namespace AccessMonitor {

    typedef unsigned long SessionID;
    typedef unsigned long ProcessID;
    typedef unsigned long ThreadID;
    typedef void* EventID;

    // Get ProcessID of current process
    ProcessID CurrentProcessID();
    // Convert OS specific ID of process to ProcessID
    ProcessID GetProcessID( unsigned int id );
    
    // Get ThreadID of current thread
    ThreadID CurrentThreadID();
    // Convert OS specific ID of thread to ThreadID
    ThreadID GetThreadID( unsigned int id );
    
    // Request access to (unique) named event for a process.
    // The event is created if it does not already exist.
    EventID AccessEvent( const std::string& tag, const SessionID session, const ProcessID process );
    // Release access to (unique) named event.
    // The event is destroyed when it is no longer accessed.
    void ReleaseEvent( EventID event );

    // Wait for an event with an otional time-out
    bool EventWait( EventID event, unsigned long milliseconds = ULONG_MAX );
    // Wait for a named event. Named event should be accessed elsewhere prior to this call.
    bool EventWait( const std::string& tag, const SessionID session, const ProcessID process, unsigned long milliseconds = ULONG_MAX );
    // Signal an event
    void EventSignal( EventID event );
    // Signal a named event. Named event should be accessed elsewhere prior to this call.
    void EventSignal( const std::string& tag, const SessionID session, const ProcessID process );

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_PROCESS_H
