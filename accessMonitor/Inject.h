#ifndef ACCESS_MONITOR_INJECT_H
#define ACCESS_MONITOR_INJECT_H

#include <string>

namespace AccessMonitor {
    typedef long int PID;
    // Inject a library in a process
    void inject( PID pid, const std::wstring& library );
}

#endif // ACCESS_MONITOR_INJECT_H
