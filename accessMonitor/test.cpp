#include "Monitor.h"
#include "Log.h"
#include "FileNaming.h"
#include "MonitorLogging.h"

#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <thread>

// ToDo: Synchronize with process exit and thread exit (via close handle) prior to stop monitoring 

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

static bool multisession = false;
static bool multithreaded = false;
static bool remoteProcess = false;

void worker( const path directoryPath ) {
    current_path( temp_directory_path() );
    debugRecord() << "std::filesystem::create_directories( " << directoryPath << " )" << record;
    create_directories( directoryPath );
    debugRecord() << "std::ofstream( " << (directoryPath / "junk.txt").c_str() << " )" << record;
    ofstream file( directoryPath / "junk.txt" );
    file << "Hello world!\n";
    file.close();
    this_thread::sleep_for(chrono::milliseconds(rand() % 17));;
    debugRecord() << "std::ofstream( " << (directoryPath / "moreJunk.txt").c_str() << " )" << record;
    ofstream anotherFile( directoryPath / "moreJunk.txt" );
    anotherFile << "Hello again!\n";
    anotherFile.close();
    debugRecord() << "Determine canonical path of " << (directoryPath / "morejunk.txt").c_str() << record;
    auto canon = canonical( directoryPath / "morejunk.txt" );
    debugRecord() << "Canaonical path is " << canon.c_str() << record;
    CopyFileW( (directoryPath / "moreJunk.txt").c_str(), (directoryPath / "evenMoreJunk.txt").c_str(), false );
    debugRecord() << "std::filesystem::remove( " << (directoryPath / "junk.txt").c_str() << " )" << record;
    remove( directoryPath / "junk.txt" );
    debugRecord() << "std::filesystem::rename( " << (directoryPath / "moreJunk.txt").c_str() << ", " << (directoryPath / "yetMorejunk.txt").c_str() << " )" << record;
    rename( directoryPath / "moreJunk.txt", directoryPath / "yetMoreJunk.txt" );
    debugRecord() << "std::filesystem::remove_all( " << directoryPath << " )" << record;
    remove_all( directoryPath );
}

void doFileAccess() {
    auto session = CurrentSessionID();
    if (remoteProcess) {
        wstringstream command;
        command
            << L"C:\\Users\\philv\\Code\\yam\\yet-another-make\\accessMonitor\\remoteTest.exe"
            << L" " 
            << ((multithreaded) ? L"true" : L"false" )
            << L" "
            << uniqueName( L"RemoteSession", session );
        auto exitCode = system( narrow( command.str() ).c_str() );
    }
    auto sessionDir = uniqueName( L"Session", session );
    if (multithreaded) {
        auto t0 = jthread( worker, path( "." ) / sessionDir / "fileAccessTest0" );
        auto t1 = jthread( worker, path( "." ) / sessionDir / "fileAccessTest1" );
        auto t2 = jthread( worker, path( "." ) / sessionDir / "fileAccessTest2" );
        auto t3 = jthread( worker, path( "." ) / sessionDir / "fileAccessTest3" );
    } else {
        worker( path( "." ) / sessionDir / "fileAccessTest" );
    }
}

void doMonitoredFileAccess() {
    startMonitoring();
    auto session = CurrentSessionID();
    doFileAccess();
    auto results( stopMonitoring() );
    // Log results...
    Log output( L"TestProgramOuptut", session  );
    for ( auto access : results ) {
        output() << access.first << L" [ " << access.second.lastWriteTime << L" ] " << modeToString( access.second.mode ) << record;
    }
    output.close();

}

bool condition( const string& argument ) {
    if ((argument == "t") || (argument == "T")) return true;
    if ((argument == "true") || (argument == "TRUE")) return true;
    return false;
}

int main( int argc, char* argv[] ) {
    multisession = false;
    multithreaded = false;
    remoteProcess = false;
    if (3 < argc) { remoteProcess = condition( argv[ 3 ] ); }
    if (2 < argc) { multithreaded = condition( argv[ 2 ] ); }
    if (1 < argc) { multisession = condition( argv[ 1 ] ); }
    if (multisession) {
        auto s0 = jthread( doMonitoredFileAccess );
        auto s1 = jthread( doMonitoredFileAccess );
        auto s2 = jthread( doMonitoredFileAccess );
        auto s3 = jthread( doMonitoredFileAccess );
    } else {
        doMonitoredFileAccess();
    }
};

