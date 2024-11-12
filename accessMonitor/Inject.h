#ifndef ACCESS_MONITOR_INJECT_H
#define ACCESS_MONITOR_INJECT_H

#include "Process.h"

#include <string>
#include <filesystem>

namespace AccessMonitor {
    // Inject (patch) library in a process
    void inject( const std::string& library, ProcessID process, SessionID session, LogAspects aspects, const std::filesystem::path& deca );
}

#endif // ACCESS_MONITOR_INJECT_H
