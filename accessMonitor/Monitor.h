#ifndef ACCESS_MONITOR_MONITOR_H
#define ACCESS_MONITOR_MONITOR_H

#include "FileAccess.h"

#include <map>

namespace AccessMonitor {

    // Collection of monitoring events, maps accessed file name to access data
    typedef std::map<std::wstring,FileAccess> MonitorEvents;

    // Start monitoring file access.
    void startMonitoring();
    // Stop monitoring file access.
    // Returns collection of monitored file accesses.
    MonitorEvents stopMonitoring();

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_H
