#include "PatchProcess.h"
#include "MonitorFiles.h"
#include "MonitorThreadsAndProcesses.h"

#include <mutex>

using namespace std;

namespace AccessMonitor {

    namespace {
        mutex patchMutex;
        unsigned int patchCount = 0;
    }

    void patchProcess() {
        const lock_guard<mutex> lock( patchMutex );
        if (patchCount == 0) {
            registerFileAccess();
            registerProcessesAndThreads();
            patch();
        }
        patchCount += 1;
    }
    void unpatchProcess() {
        const lock_guard<mutex> lock( patchMutex );
        patchCount -= 1;
        if (patchCount == 0) {
            unpatch();
            unregisterFileAccess();
            unregisterProcessCreation();
        }
    }

} // namespace AccessMonitor
