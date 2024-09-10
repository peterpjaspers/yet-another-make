#ifndef ACCESS_MONITOR_MONITOR_H
#define ACCESS_MONITOR_MONITOR_H

#include "Session.h"
#include "FileAccess.h"
#include "MonitorLogging.h"

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
    // The aspects argument defines which aspects to log.
    void startMonitoring( std::filesystem::path const& directory = "", const LogAspects aspects = (PatchExecution | FileAccesses) );
    // Stop monitoring file access.
    // Returns collection of monitored file accesses.
    MonitorEvents stopMonitoring();

    // A MonitorGuard prevents recursive entry into monitoring code; i.e., a patched function called
    // from monitoring code associated with a patched function will not itself be monitored. Effectively
    // only patched functions called from the application are actually monitored. The guard also esures
    // that the monitoring session is initialized (during the first call to a patched function in a
    // process spawned in a monitoring session).
    class MonitorGuard {
    private:
        MonitorAccess* access;
    public:
        MonitorGuard() = delete;
        MonitorGuard( MonitorAccess* monitor );
        ~MonitorGuard();
        bool monitoring();
    };

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_H
