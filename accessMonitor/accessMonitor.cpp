
#define _CRT_SECURE_NO_WARNINGS

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
        //bool go = true;
        //while (go) {
        //    bool stop = true;
        //}
        std::filesystem::path sessionDirectory = std::filesystem::temp_directory_path();
        ProcessID process = CurrentProcessID();
        SessionID session;
        ThreadID mainThread; // Main thread ID of (remote) process
        retrieveSessionInfo( sessionDirectory, process, session, mainThread );
        CreateRemoteSession(sessionDirectory, session, process, mainThread );
        auto requestExit = AccessEvent( "RequestExit", session, process );
        auto processPatched = AccessEvent( "ProcessPatched", session, process );
        SessionDebugLog( createDebugLog() );
        SessionEventLog( createEventLog() );
        debugLog().enable( PatchedFunction | ParseLibrary | PatchExecution | FileAccesses );
        debugRecord() << "Start monitoring session " << session << " in process " << process << "..." << dec << record;
        patchProcess();
        EventSignal( processPatched ); // Signal (parent) process that monitoring has started
        EventWait( requestExit ); // Wait for process to (request) exit before exitting monitor thread
        debugRecord() << "Stop monitoring session " << session << " in process " << process << "..." << dec << record;
        unpatchProcess();
        remove( sessionInfoPath(sessionDirectory, process ) );
        SessionEventLogClose();
        SessionDebugLogClose();
        EventSignal( "ExitProcess", session, process ); // Signal process that it may exit
        ReleaseEvent( processPatched );
        ReleaseEvent( requestExit );
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
