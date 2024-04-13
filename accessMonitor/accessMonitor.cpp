#include "Process.h"
#include "PatchProcess.h"
#include "Session.h"
#include "FileNaming.h"
#include "MonitorLogging.h"

#include <windows.h>

// ToDo: Understand why Stop monitoring debug message is not present in CMD process

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

namespace {

    DWORD monitorDLLMain( void* argument ) {
        ProcessID process = CurrentProcessID();
        SessionID session;
        ThreadID mainThread; // Main thread ID of (remote) process
        retrieveSessionInfo( process, session, mainThread );
        CreateRemoteSession( session, process, mainThread );
        SessionDebugLog( createDebugLog() );
        SessionEventLog( createEventLog() );
        debugLog().enable( PatchedFunction | ParseLibrary | PatchExecution | FileAccesses );
        debugRecord() << "Start monitoring session " << session << " in process " << process << "..." << dec << record;
        patchProcess();
        auto requestExit = AccessEvent( "RequestExit", session, process );
        EventSignal( "ProcessPatched", session, process ); // Signal (parent) process that monitoring has started
        EventWait( requestExit ); // Wait for process to (request) exit before exitting monitor thread
        ReleaseEvent( requestExit );
        debugRecord() << "Stop monitoring session " << session << " in process " << process << "..." << dec << record;
        unpatchProcess();
        remove( sessionInfoPath( process ) );
        SessionEventLogClose();
        SessionDebugLogClose();
        EventSignal( "ExitProcess", session, process ); // Signal process that it may exit
        return ERROR_SUCCESS;
    }

}

BOOL WINAPI DllMain( HINSTANCE dll,  DWORD reason, LPVOID arg ) {
    if (reason == DLL_PROCESS_ATTACH) {
        auto monitorThread = CreateThread( nullptr, 0, monitorDLLMain, nullptr, 0, nullptr );
        if (monitorThread == nullptr) return false;
    } else if (reason == DLL_THREAD_ATTACH) {
    } else if (reason == DLL_THREAD_DETACH) {
    } else if (reason == DLL_PROCESS_DETACH) {
        if (arg != nullptr) return true;
    }
    return true;
}
