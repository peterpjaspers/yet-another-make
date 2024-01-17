#include "Monitor.h"
#include "FileAccessEvents.h"
#include "Log.h"

#include <windows.h>
#include <iostream>
#include <fstream>

// #pragma comment(lib, "User32")

using namespace AccessMonitor;
using namespace std;

DWORD monitorMain( void* argument ) {
    auto monitor = AccesstEvent( "Monitor", CurrentProcessID() );
    auto exit = AccesstEvent( "ExitProcess", CurrentProcessID() );
    enableLog( "patch", Verbose );
    log() << "Start monitoring..." << endLine;
    startMonitoring();
    EventSignal( monitor ); // Signal parent process that monitoring has started
    // Wait for this process to exit
    if (EventWait( exit )) {
        log() << "Stop monitoring..." << endLine;
        stopMonitoring();
        wofstream files( "./remoteAccessedFiles.txt" );
        streamAccessedFiles( files );
        files.close();
        log() << "Done monitoring..." << endLine;
        disableLog();
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
