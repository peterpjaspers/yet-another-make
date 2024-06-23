#ifndef ACCESS_MONITOR_MONITOR_H
#define ACCESS_MONITOR_MONITOR_H

#include "FileAccess.h"

#include <filesystem>
#include <map>

namespace AccessMonitor {

    // Collection of monitoring events, maps accessed file name to access data
    typedef std::map<std::wstring,FileAccess> MonitorEvents;

    // Start monitoring file access.
    // If directory != "": store monitor data in 'directory'. 
    // If directory == "": store data in std::filesystem::temp_directory_path.
    // Take care: child processes spawned between start and stop monitoring 
    // will store data in std::filesystem::temp_directory_path. For proper 
    // functioning the spawned processes must use same directory as the parent 
    // process. Caller must ensure this by setting variables TMP and TEMP in
    // the environment of the spawned process to the directory used by the 
    // parent process.
    void startMonitoring(std::filesystem::path const& directory = "");
    // Stop monitoring file access.
    // Returns collection of monitored file accesses.
    MonitorEvents stopMonitoring();

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_H
