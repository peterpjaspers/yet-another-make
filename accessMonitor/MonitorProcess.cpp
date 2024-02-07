#include "MonitorProcess.h"
#include "Patch.h"
#include "Process.h"
#include "MonitorFiles.h"
#include "MonitorThreadsAndProcesses.h"

#include <windows.h>
#include <sstream>
#include <fstream>
#include <filesystem>

// ToDo: Add function calling convention to all externals

using namespace std;
using namespace std::filesystem;

namespace AccessMonitor {

    namespace {
        const wstring patchDLLFile( L"C:\\Users\\philv\\Code\\yam\\yet-another-make\\accessMonitor\\patchDLL.dll" );
        static bool monitoring = false;
    }

    Log monitorEvents;

    void startMonitoringProcess() {
        static const char* signature = "void AccessMonitor::startMonitoringProcess()";
        if (monitoring) throw string( signature ) + " - Already monitoring!";
        monitoring = true;
        monitorEvents = Log( "AccessMonitorData/Monitor_Events_" );
        registerFileAccess();
        registerProcessesAndThreads();
        patch();
    }
    void stopMonitoringProcess() {
        static const char* signature = "void AccessMonitor::stopMonitoringProcess()";
        if (!monitoring) throw string( signature ) + " - Not monitoring!";
        unpatch();
        unregisterFileAccess();
        unregisterProcessCreation();
        monitorEvents.close();
        monitoring = false;
    }

} // namespace AccessMonitor
