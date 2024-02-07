#include "MonitorProcess.h"
#include "Log.h"

#include <windows.h>
#include <iostream>
#include <fstream>

using namespace AccessMonitor;
using namespace std;

DWORD monitorMain( void* argument ) {
    auto monitor = AccessEvent( "Monitor", CurrentProcessID() );
    auto exit = AccessEvent( "ExitProcess", CurrentProcessID() );
    monitorLog.enable( PatchedFunction | ParseLibrary | PatchExecution | FileAccess );
    monitorLog() << "Start monitoring..." << record;
    startMonitoringProcess();
    EventSignal( monitor ); // Signal parent process that monitoring has started
    // Wait for this process to exit
    if (EventWait( exit )) {
        monitorLog() << "Stop monitoring..." << record;
        stopMonitoringProcess();
        EventSignal( monitor ); // Signal parent process that monitoring has stopped
        EventSignal( exit ); // Signal this process that it can exit
    }
    ReleaseEvent( monitor );
    ReleaseEvent( exit );
    return ERROR_SUCCESS;
}

BOOL WINAPI DllMain( HINSTANCE dll,  DWORD reason, LPVOID arg ) {
    if (reason == DLL_PROCESS_ATTACH) {
        auto thread = CreateThread( nullptr, 0, monitorMain, nullptr, 0, nullptr );
        if (thread == nullptr) return false;
    } else if (reason == DLL_THREAD_ATTACH) {
    } else if (reason == DLL_THREAD_DETACH) {
    } else if (reason == DLL_PROCESS_DETACH) {
        if (arg != nullptr) return true;
    }
    return true;
}
