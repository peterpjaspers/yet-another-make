#include "Monitor.h"
#include "Patch.h"
#include "MonitorFiles.h"
#include "MonitorThreadsAndProcesses.h"

// ToDo: Add function calling convention to all externals

using namespace std;

namespace AccessMonitor {

    namespace {
        static bool monitoring = false;
    }

    void startMonitoring() {
        static const char* signature = "void startMonitoring()";
        if (monitoring) throw string( signature ) + " - Already monitoring!";
        monitoring = true;
        registerFileAccess();
        registerProcessesAndThreads();
        patch();
    }

    void startMonitoring( ProcessID process ) {
        static const char* signature = "void startMonitoring( ProcessID process )";
        if (monitoring) throw string( signature ) + " - Already monitoring!";
        // ToDo: Implement this function (monitoring of a specific process)
        startMonitoring();

    }

    void stopMonitoring() {
        static const char* signature = "void stopMonitoring()";
        if (!monitoring) throw string( signature ) + " - Not monitoring!";
        unpatch();
        unregisterFileAccess();
        unregisterProcessCreation();
        monitoring = false;
    }

} // namespace AccessMonitor
