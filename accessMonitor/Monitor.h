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
    // Session related result files are store in the given directory.
    // The aspects argument defines which (debug) aspects to log.
    void startMonitoring( const std::filesystem::path& directory, const LogAspects aspects = (PatchExecution | FileAccesses) );
    // Start monitoring file access in a remote process.
    // Extends the session referred to by context to the remote process.
    void startMonitoring( const SessionContext& context );
    // Stop monitoring file access.
    // Returns collection of monitored file accesses.
    void stopMonitoring( MonitorEvents* events );

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
        inline bool operator()() { return( (access != nullptr) && (access->monitorCount == 1) ); }
        inline unsigned long count() { return access->monitorCount; }
        inline unsigned long error() { return access->errorCode; }
    };

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_H
