
#define _CRT_SECURE_NO_WARNINGS

#include "../Process.h"
#include "../Session.h"
#include "../FileNaming.h"
#include "../MonitorLogging.h"
#include "../MonitorFiles.h"
#include "../MonitorThreadsAndProcesses.h"
#include <windows.h>

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

namespace {

    void exitHandler() {
        ProcessID process = CurrentProcessID();
        SessionID session = CurrentSessionID();
        debugRecord() << "Stop monitoring session " << session << " in process " << process << "..." << dec << record;
        remove( sessionInfoPath( temp_directory_path(), process ) );
        SessionEventLogClose();
        SessionDebugLogClose();
    }

} // namespace

BOOL WINAPI DllMain( HINSTANCE dll,  DWORD reason, LPVOID arg ) {
    if (reason == DLL_PROCESS_ATTACH) {
        registerFileAccess();
        registerProcessesAndThreads();
        patch();
        atexit( exitHandler );
    } else if (reason == DLL_PROCESS_DETACH) {
        unpatch();
    }
}
