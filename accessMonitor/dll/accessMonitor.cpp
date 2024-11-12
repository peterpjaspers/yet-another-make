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
    /*while (true) {
        bool stop = true;
    }*/
    if (reason == DLL_PROCESS_ATTACH) {
        ProcessID process( CurrentProcessID() );
        SessionID session;
        LogAspects aspects;
        path directory;
        retrieveSessionContext( process, session, aspects, directory );
        startMonitoring( directory, session, aspects );
        EventSignal( "ProcessPatched", process );
    } else if (reason == DLL_PROCESS_DETACH) {
        stopMonitoring( nullptr );
    }
    return true;
}
