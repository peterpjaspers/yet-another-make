#include "Patch.h"
#include "FileNaming.h"
#include "MonitorLogging.h"
#include "Inject.h"

#include <windows.h>

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

namespace {


    DWORD monitorDLLMain( void* argument ) {
        ProcessID process = CurrentProcessID();
        SessionID session;
        ThreadID mainThread; // Main thread ID of (remote) process
        retrieveSessionInfo( process, session, mainThread );
        ThreadID monitorThread = CurrentThreadID();
        // Simply add monitor thread and main thread to session adminstration, session was created by ancestor process
        AddSessionThread( monitorThread, session );
        AddSessionThread( mainThread, session );
        auto processPatched = AccessEvent( "ProcessPatched", session, process );
        auto processExit = AccessEvent( "ProcessExit", session, process );
        createEventLog( session );
        monitorLog.enable( PatchedFunction | ParseLibrary | PatchExecution | FileAccesses );
        monitorLog() << "Start monitoring in process 0x" << hex << process << "..." << record;
        patchProcess();
        SetSessionState( SessionActive );
        EventSignal( processPatched ); // Signal (parent) process that monitoring has started
        EventWait( processExit ); // Wait for process to exit before exitting monitor thread
        ReleaseEvent( processPatched );
        ReleaseEvent( processExit );
        RemoveSessionThread( monitorThread );
        RemoveSessionThread( mainThread );
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
