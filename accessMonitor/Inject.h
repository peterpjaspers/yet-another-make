#ifndef ACCESS_MONITOR_INJECT_H
#define ACCESS_MONITOR_INJECT_H

#include "Process.h"

#include <string>
#include <filesystem>

namespace AccessMonitor {
    // Inject (patch) library in a process
    void inject(std::filesystem::path const& sessionDirectory, SessionID session, ProcessID process, ThreadID thread, const std::wstring& library );
}

#endif // ACCESS_MONITOR_INJECT_H
