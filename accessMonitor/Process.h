#ifndef ACCESS_MONITOR_PROCESS_H
#define ACCESS_MONITOR_PROCESS_H

#include <string>

namespace AccessMonitor {

    typedef unsigned long ProcessID;
    typedef void* EventID;

    // Get ProcessID of current process
    ProcessID CurrentProcessID();
    // Convert OS specific ID of process to ProcessID
    ProcessID GetProcessID( unsigned long id );
    
    // Request access to (unique) named event for a process.
    // The event is created if it does not already exist.
    EventID AccesstEvent( const std::string& tag, const ProcessID pid );
    // Release access to (unique) named event for a process.
    // The event is destroyed when it is no longer accessed.
    void ReleaseEvent( EventID event );

    // Wait for an event
    bool EventWait( EventID event, unsigned long milliseconds = ULONG_MAX );
    // Signal an event
    void EventSignal( EventID event );

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_PROCESS_H
