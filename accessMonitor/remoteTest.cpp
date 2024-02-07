#include "Process.h"
#include "Log.h"

#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace std;
using namespace std::filesystem;
using namespace AccessMonitor;

Log debugLog;

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
    debugLog() << "Remote test do file access" << record;
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

int main() {
    debugLog = Log( "RemoteTest", true, true );
    debugLog() << "Remote test main begin" << record;
    try {
        doFileAccess( true );
    }
    catch (...) {
        debugLog() << "Remote test exception" << record;
        ofstream file( "./exception.txt" );
        file << "Exception caught!\n";
        file.close();
    }
    debugLog() << "Remote test main end" << record;
};

