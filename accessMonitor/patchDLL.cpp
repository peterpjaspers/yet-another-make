#include "Process.h"
#include "PatchProcess.h"
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
        ThreadID monitorThread = CurrentThreadID();
        // Simply add monitor thread and main thread to session adminstration, session was created by ancestor process
        AddSessionThread( monitorThread, session );
        AddSessionThread( mainThread, session );
        createEventLog();
        createDebugLog();
        debugLog().enable( PatchedFunction | ParseLibrary | PatchExecution | FileAccesses );
        debugRecord() << "Start monitoring in process 0x" << hex << process << "..." << record;
        SetSessionState( SessionActive );
        patchProcess();
        auto requestExit = AccessEvent( "RequestExit", session, process );
        EventSignal( "ProcessPatched", session, process ); // Signal (parent) process that monitoring has started
        EventWait( requestExit ); // Wait for process to (request) exit before exitting monitor thread
        ReleaseEvent( requestExit );
        debugRecord() << "Stop monitoring in process 0x" << hex << process << "..." << record;
        unpatchProcess();
        remove( sessionInfoPath( process ) );
        closeEventLog();
        closeDebugLog();
        EventSignal( "ExitProcess", session, process ); // Signal process that it may exit
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
