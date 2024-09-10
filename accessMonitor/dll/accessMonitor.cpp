
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

//     DWORD monitorDLLMain( void* argument ) {
//         path sessionDirectory = temp_directory_path();
//         ProcessID process = CurrentProcessID();
//         SessionID session;
//         ThreadID mainThread; // Main thread ID of (remote) process
//         retrieveSessionInfo( sessionDirectory, process, session, mainThread );
//         CreateRemoteSession( sessionDirectory, session, process, mainThread );
//         auto processPatched = AccessEvent( "ProcessPatched", session, process );
//         SessionDebugLog( createDebugLog() );
//         SessionEventLog( createEventLog() );
//         debugLog().enable( PatchedFunction | ParseLibrary | PatchExecution | FileAccesses );
//         debugRecord() << "Start monitoring session " << session << " in process " << process << "..." << dec << record;
//         patchProcess();
//         EventSignal( processPatched ); // Signal (parent) process that monitoring has started
//         ReleaseEvent( processPatched );
//         return ERROR_SUCCESS;
//     }

    void exitHandler() {
        ProcessID process = CurrentProcessID();
        SessionID session = CurrentSessionID();
        debugRecord() << "Stop monitoring session " << session << " in process " << process << "..." << dec << record;
        remove( sessionInfoPath( temp_directory_path(), process ) );
        SessionEventLogClose();
        SessionDebugLogClose();
    }

} // namespace

// BOOL WINAPI DllMain( HINSTANCE dll,  DWORD reason, LPVOID arg ) {
//     if (reason == DLL_PROCESS_ATTACH) {
//         auto monitorThread = CreateThread( nullptr, 0, monitorDLLMain, nullptr, 0, nullptr );
//         if (monitorThread == nullptr) return false;
//         atexit( exitHandler );
//     }
//     return true;
// }

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
