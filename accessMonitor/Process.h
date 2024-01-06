#ifndef ACCESS_MONITOR_PROCESS_H
#define ACCESS_MONITOR_PROCESS_H

namespace AccessMonitor {

    typedef unsigned long ProcessID;

    // Get ProcessID of current process
    ProcessID CurrentProcessID();
    // Convert OS specific ID of process to ProcessID
    ProcessID GetProcessID( DWORD id );
    

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_PROCESS_H
