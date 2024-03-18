#ifndef ACCESS_MONITOR_INJECT_H
#define ACCESS_MONITOR_INJECT_H

#include "Process.h"

#include <string>

namespace AccessMonitor {
    // Inject (patch) library in a process
    void inject( SessionID session, ProcessID process, ThreadID thread, const std::wstring& library );
}

#endif // ACCESS_MONITOR_INJECT_H
