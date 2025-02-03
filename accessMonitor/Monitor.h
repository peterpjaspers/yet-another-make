#ifndef ACCESS_MONITOR_MONITOR_H
#define ACCESS_MONITOR_MONITOR_H

#include "Session.h"
#include "FileAccess.h"
#include "MonitorLogging.h"

#include <filesystem>
#include <map>
#include <windows.h>

namespace AccessMonitor {

    // Collection of monitoring events, maps accessed file name to access data
    typedef std::map<std::filesystem::path,FileAccess> MonitorEvents;

    // Enable monitoring in this process.
    // Actual monitoring will be performed between calls to startMonitoring and stopMonitoring.
    void enableMonitoring();
    // Disable monitoring in this process.
    // Should not be called while monitoring is effect.
    void disableMonitoring();
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
    // from monitoring code associated with a patched function will not itself be monitored.
    // Effectively, only patched function calls invoked directly from the application are monitored.
    class MonitorGuard {
    private:
        MonitorAccess* access;
        bool restoreError;
    public:
        MonitorGuard() = delete;
        inline MonitorGuard( MonitorAccess* monitor, bool error = true ) : access( monitor ), restoreError( error ) {
            if (access != nullptr) {
                auto count( access->monitorCount++ );
                if ((count == 0) && restoreError) access->errorCode = GetLastError();
            }
        }
        inline ~MonitorGuard() {
            if (access != nullptr) {
                auto count( --access->monitorCount );
                if ((count == 0) && restoreError) SetLastError( access->errorCode );
            }
        }
        inline bool operator()() { return( (access != nullptr) && (access->monitorCount == 1 ) ); }
        inline unsigned long count() { return( (access != nullptr) ? access->monitorCount : 0 ); }
        inline unsigned long error() { return( (access != nullptr) ? access->errorCode : 0 ); }
    };

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_MONITOR_H
