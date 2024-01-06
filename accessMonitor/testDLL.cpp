#include <windows.h>
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

// Test the patch DLL by simply loading it (without injection in a process)...

int main( int argc, char* argv[] ) {
    HMODULE module = LoadLibraryW( L"C:\\Users\\philv\\Code\\yam\\yet-another-make\\accessMonitor\\patchDLL.dll" );
    if (module == nullptr) {
        cout << "DLL load failed with error code " << GetLastError() << "!" << endl;
    }
    // Allow DLL monitor thread to execute before exiting process...
    while (true) this_thread::sleep_for( 2000ms );
 }
 