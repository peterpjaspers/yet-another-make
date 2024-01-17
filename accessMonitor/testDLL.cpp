#include "Process.h"

#include <windows.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace std;
using namespace AccessMonitor;

// Test the patch DLL by simply loading it (without injection in a process)...

int main( int argc, char* argv[] ) {
    auto monitor = AccesstEvent( "Monitor", CurrentProcessID() );
    auto exit = AccesstEvent( "ExitProcess", CurrentProcessID() );
    HMODULE module = LoadLibraryW( L"C:\\Users\\philv\\Code\\yam\\yet-another-make\\accessMonitor\\patchDLL.dll" );
    if (module == nullptr) {
        cout << "DLL load failed with error code " << GetLastError() << "!" << endl;
    }
    // Allow DLL monitor thread to execute before exiting process...
    if (EventWait( monitor )) {
        ofstream file( "./junk.txt" );
        file << "Hello world!\n";
        file.close();
        EventSignal( exit );
        EventWait( monitor );
        EventWait( exit );
    }
    ReleaseEvent( monitor );
    ReleaseEvent( exit );
}
 