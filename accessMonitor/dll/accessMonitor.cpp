#define _CRT_SECURE_NO_WARNINGS

#include "../FileNaming.h"
#include "../Session.h"
#include "../Monitor.h"

#include <windows.h>
#include <psapi.h>

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

BOOL WINAPI DllMain( HINSTANCE dll,  DWORD reason, LPVOID arg ) {
    if (reason == DLL_PROCESS_ATTACH) {
        //while (true) {
        //    bool stop = true;
        //}
        ProcessID process( CurrentProcessID() );
        enableMonitoring();
        startMonitoring( Session::retrieveContext( process ) );
        EventSignal( "ProcessPatched", process );
    } else if (reason == DLL_PROCESS_DETACH) {
        // Commented-out because they cause a crash
        // Also these statements are not necessary
        // because detach happens during process exit. 
        //stopMonitoring( nullptr );
        //disableMonitoring();
    }
    return true;
}
