#include "Monitor.h"
#include "Log.h"
#include "FileNaming.h"

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

void worker( const path directoryPath ) {
    current_path( temp_directory_path() );
    create_directories( directoryPath );
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
        auto t = jthread( worker, path( "." ) / sessionDir / "fileAccessTest" );
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
    Log output( L"TestProgramOuptut", session, L"txt" );
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
    multithreaded = false;
    remoteProcess = false;
    if (2 < argc) { remoteProcess = condition( argv[ 2 ] ); }
    if (1 < argc) { multithreaded = condition( argv[ 1 ] ); }
    auto t0 = jthread( doMonitoredFileAccess );
    auto t1 = jthread( doMonitoredFileAccess );
};

