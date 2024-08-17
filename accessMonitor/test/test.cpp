#include "../Monitor.h"
#include "../Session.h"
#include "../LogFile.h"
#include "../FileNaming.h"
#include "../MonitorLogging.h"

#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <thread>
#include <cstdlib>

// ToDo: Figure out why program crashes when not all file from previous run are deleted.

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

static int sessions = 1;
static int threads = 1;
static bool remoteProcess = false;

const std::wstring remoteTestFile( L"C:/Users/philv/Code/yam/yet-another-make/accessMonitor/test/remoteTest.exe" );
// const std::wstring remoteTestFile( L"D:/Peter/Github/yam/x64/Debug/remoteTest.exe" );

void worker( const path directoryPath ) {
    try {
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
        error_code ec;
        remove_all( directoryPath, ec );
        debugRecord() << "std::filesystem::create_directories( " << directoryPath << " )" << record;
        create_directories( directoryPath );
        file = ofstream( directoryPath / "junk.txt" );
        file << "Hello world!\n";
        file.close();
    }
    catch (filesystem_error const& exception) {
        debugRecord() << "Exception  " << exception.what() << "!" << record;
    }
    catch (...) {
        debugRecord() << "Exception!" << record;
    }
}

void doFileAccess() {
    auto session = CurrentSessionID();
    if (remoteProcess) {
        wstringstream command;
        command
            << remoteTestFile
            << L" " 
            << threads
            << L" "
            << uniqueName( L"RemoteSession", session );
        auto exitCode = system( narrow( command.str() ).c_str() );
    }
    auto sessionDir = uniqueName( L"Session", session );
    if (1 < threads) {
        vector<thread> workerThreads;
        for (int i = 0; i < threads; ++i) {
            wstringstream subdir;
            subdir << L"fileAccessTest" << i;
            workerThreads.push_back( thread( worker, path( "." ) / sessionDir / subdir.str() ) );
        }
        for (int i = 0; i < threads; ++i) workerThreads[ i ].join();
    } else {
        worker( path( "." ) / sessionDir / L"fileAccessTest" );
    }
}

void doMonitoredFileAccess() {
    startMonitoring( "", PatchedFunction | PatchExecution | FileAccesses | WriteTime );
    auto session = CurrentSessionID();
    doFileAccess();
    auto results( stopMonitoring() );
    // LogFile results...
    LogFile output( L"TestProgramOutput", session  );
    for ( auto access : results ) {
        output() << access.first << L" [ " << access.second.lastWriteTime << L" ] " << modeToString( access.second.modes ) << " : " << modeToString( access.second.mode ) << record;
    }
    output.close();

}

bool condition( const string& argument ) {
    if ((argument == "t") || (argument == "T")) return true;
    if ((argument == "true") || (argument == "TRUE")) return true;
    return false;
}

int main( int argc, char* argv[] ) {
    sessions = 1;
    threads = 1;
    remoteProcess = false;
    if (3 < argc) { remoteProcess = condition( argv[ 3 ] ); }
    if (2 < argc) { threads = atoi( argv[ 2 ] ); }
    if (threads <= 1) threads = 1;
    if (1 < argc) { sessions = atoi( argv[ 1 ] ); }
    if (sessions <= 1) sessions = 1;
    vector<thread> sessionThreads;
    for (int i = 0; i < sessions; ++i) sessionThreads.push_back( thread( doMonitoredFileAccess ) );
    for (int i = 0; i < sessions; ++i) sessionThreads[ i ].join();
};

