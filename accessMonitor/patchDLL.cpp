#include "Monitor.h"
#include "FileAccessEvents.h"
#include "Log.h"

#include <windows.h>
#include <iostream>
#include <fstream>

using namespace AccessMonitor;
using namespace std;

DWORD threadMain( void* argument ) {
    enableLog( "patch", Verbose );
    log() << "Start monitoring..." << endLine;
    startMonitoring();
    // ToDo: Signal parent process that monitoring has started
    HMODULE dll;
    if ( GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, LPCSTR( threadMain ), &dll ) ) {
        wchar_t moduleName[ MaxFileName + 1 ];
        DWORD size = GetModuleFileNameW( dll, &moduleName[ 0 ], MaxFileName );
        if (0 < size) {
            wofstream file( "C:/Users/philv/Code/yam/yet-another-make/accessMonitor/InjectOutput.txt" );
            file << L"Loaded module " << moduleName << L"..." << endl;
            file.close();
        }
    }
    // ToDo: Delay process exit/terminate until accessed file data is communicated
    log() << "Stop monitoring..." << endLine;
    stopMonitoring();
    wofstream files( "./remoteAccessedFiles.txt" );
    streamAccessedFiles( files );
    files.close();
    log() << "Done..." << endLine;
    disableLog();
    // ToDo: Signal parent process that monitoring has completed
    return( ERROR_SUCCESS );
}

BOOL WINAPI DllMain( HINSTANCE dll,  DWORD reason, LPVOID arg ) {
    switch( reason ) { 
        case DLL_PROCESS_ATTACH:
            CreateThread( nullptr, 0, threadMain, nullptr, 0, nullptr );
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            if (arg != nullptr) break;
            break;
    }
    return TRUE;
}
