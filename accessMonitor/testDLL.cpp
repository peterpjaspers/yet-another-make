#include "Process.h"

#include <windows.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace std;
using namespace std::filesystem;
using namespace AccessMonitor;

// Test the patch DLL by simply loading it (without injection in a process)...

void worker( const path directoryPath ) {
    create_directory( directoryPath, current_path() );
    ofstream file( directoryPath / "junk.txt" );
    file << "Hello world!\n";
    file.close();
    this_thread::sleep_for(chrono::milliseconds(rand() % 17));;
    ofstream anotherFile( directoryPath / "moreJunk.txt" );
    anotherFile << "Hello again!\n";
    anotherFile.close();
    CopyFileW( (directoryPath / "moreJunk.txt").c_str(), (directoryPath / "evenMoreJunk.txt").c_str(), false );
    remove( directoryPath / "junk.txt" );
    rename( directoryPath / "moreJunk.txt", directoryPath / "yetMoreJunk.txt" );
    remove_all( directoryPath );
}

void doFileAccess( bool multithreaded = true ) {
    if (multithreaded) {
        auto t = jthread( worker, path( "./remoteFileAccessTest" ) );
        auto t0 = jthread( worker, path( "./remoteFileAccessTest0" ) );
        auto t1 = jthread( worker, path( "./remoteFileAccessTest1" ) );
        auto t2 = jthread( worker, path( "./remoteFileAccessTest2" ) );
        auto t3 = jthread( worker, path( "./remoteFileAccessTest3" ) );
        t.join();
        t0.join();
        t1.join();
        t2.join();
        t3.join();
    } else {
        worker( path( "./remoteFileAccessTest" ) );
    }
}

int main( int argc, char* argv[] ) {
    const wstring patchDLLFile( L"C:\\Users\\philv\\Code\\yam\\yet-another-make\\accessMonitor\\patchDLL.dll" );
    // Manually inject path DLL for debugging...
    HMODULE module = LoadLibraryW( L"C:\\Users\\philv\\Code\\yam\\yet-another-make\\accessMonitor\\patchDLL.dll" );
    if (module == nullptr) cout << "DLL load failed with error code " << GetLastError() << "!" << endl;
    auto monitor = AccessEvent( "Monitor", CurrentProcessID() );
    auto exit = AccessEvent( "ExitProcess", CurrentProcessID() );
    EventWait( monitor );
    try {
        doFileAccess( true );
    }
    catch (...) {
        ofstream file( "./exception.txt" );
        file << "Exception caught!\n";
        file.close();
    }
    EventSignal( exit );
    EventWait( monitor );
    ReleaseEvent( monitor );
    ReleaseEvent( exit );
}