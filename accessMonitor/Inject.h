#ifndef ACCESS_MONITOR_INJECT_H
#define ACCESS_MONITOR_INJECT_H

#include "Process.h"
#include "Session.h"

#include <string>
#include <filesystem>

namespace AccessMonitor {
    // Inject (patch) library in a process
    void inject( const std::string& library, ProcessID process, const Session* session );
}

#endif // ACCESS_MONITOR_INJECT_H
