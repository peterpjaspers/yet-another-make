#define _CRT_SECURE_NO_WARNINGS

#include "../FileNaming.h"
#include "../Session.h"
#include "../Monitor.h"

#include <windows.h>
#include <psapi.h>

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

inline wostream& debugMessage() { return debugRecord() << L"AccessMonitor - Exitting process with ID " << CurrentProcessID() << L", executable "; }

BOOL WINAPI DllMain( HINSTANCE dll,  DWORD reason, LPVOID arg ) {
    if (reason == DLL_PROCESS_ATTACH) {
        ProcessID process( CurrentProcessID() );
        SessionID session;
        LogAspects aspects;
        path directory;
        retrieveSessionContext( process, session, aspects, directory );
        startMonitoring( directory, session, aspects );
        EventSignal( "ProcessPatched", process );
    } else if (reason == DLL_PROCESS_DETACH) {
        if (debugLog( PatchExecution )) {
            char fileName[ MaxFileName ] = "";
            int size = 0;
            HANDLE handle = OpenProcess( READ_CONTROL, false, GetCurrentProcessId() );
            if (handle != NULL) {
                // ToDo: Understand how to retrieve process executable name 
                size = GetModuleFileNameExA( handle, NULL, fileName, MaxFileName );
                CloseHandle( handle );
                if (size == 0) {
                    debugMessage() << L"path could not be determined [ " << GetLastError() << L" ]" << record;
                } else {
                    debugMessage() << fileName << record;
                }
            } else {
                debugMessage() << L"unknown, could not access process [ " << GetLastError() << L" ]" << record;
            }
        }
        stopMonitoring( nullptr );
    }
    return true;
}
