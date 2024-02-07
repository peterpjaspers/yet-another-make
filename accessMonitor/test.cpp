#include "Monitor.h"
#include "MonitorFiles.h"
#include "FileAccessEvents.h"
#include "Log.h"

#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

static bool multithreaded = false;
static bool remoteProcess = false;

Log logger;

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

void doFileAccess() {
    if (remoteProcess) {
        wstring command = L"C:\\Users\\philv\\Code\\yam\\yet-another-make\\accessMonitor\\remoteTest.exe";
        auto exitCode = system( narrow( command ).c_str() );
        logger() << "Process " << command << " exitted with " << hex << uppercase << noshowbase << static_cast<uint32_t>( exitCode ) << dec << record;
    }
    if (multithreaded) {
        auto t = jthread( worker, path( "./fileAccessTest" ) );
        auto t0 = jthread( worker, path( "./fileAccessTest0" ) );
        auto t1 = jthread( worker, path( "./fileAccessTest1" ) );
        auto t2 = jthread( worker, path( "./fileAccessTest2" ) );
        auto t3 = jthread( worker, path( "./fileAccessTest3" ) );
    } else {
        worker( path( "./fileAccessTest" ) );
    }
}

bool condition( const string& argument ) {
    if ((argument == "t") || (argument == "T")) return true;
    if ((argument == "true") || (argument == "TRUE")) return true;
    return false;
}

int main( int argc, char* argv[] ) {
    multithreaded = false;
    remoteProcess = false;
    if (2 < argc) { remoteProcess = condition( argv[ 2 ] ); }
    if (1 < argc) { multithreaded = condition( argv[ 1 ] ); }
    try {
        logger = Log( "TestLog", false, true );
        logger.enable( PatchedFunction | ParseLibrary | PatchExecution | FileAccess );
        logger() << "Performing file access without monitoring..." << record;
        doFileAccess();
        logger() << "Start monitoring..." << record;
        startMonitoring();
        logger() << "Performing file access with monitoring..." << record;
        doFileAccess();
        logger() << "Stop monitoring..." << record;
        stopMonitoring();
        logger() << "Performing file access without monitoring..." << record;
        doFileAccess();
        logger() << "Done..." << record;
    }
    catch ( string message ) {
        logger() << widen( message ) << record;
    }
    catch (...) {
        logger() << "Exception!" << record;
    }
};

