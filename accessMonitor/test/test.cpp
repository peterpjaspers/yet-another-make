#include "../Monitor.h"
#include "../Session.h"
#include "../LogFile.h"
#include "../FileNaming.h"
#include "../MonitorLogging.h"

#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <cstdlib>

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

static int sessions = 1;        // Number of simultaneous sessions
static int threads = 1;         // Number of threads in each session
static int iterations = 1;      // Number of successive start/stop monitoring requests in each session
static bool remoteProcess = false;

// Execute a command in a seperate process...
void executeCommand( const char* command ) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    std::memset( &si, 0, sizeof(si) );
    si.cb = sizeof(si);
    std::memset( &pi, 0, sizeof(pi) );
    // Create the process with all settings to default values...
    CreateProcessA(
        nullptr,
        const_cast<char*>( command ),
        nullptr,
        nullptr,
        false,
        0,
        nullptr,
        nullptr,
        &si,
        &pi
    );
    // Wait for the process to complete...
    WaitForSingleObject( pi.hProcess, INFINITE );
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
}

const std::string remoteTestFile( "C:/Users/philv/Code/yam/yet-another-make/accessMonitor/test/remoteTest.exe" );
// const std::wstring remoteTestFile( L"D:/Peter/Github/yam/x64/Debug/remoteTest.exe" );

void worker( const path directoryPath ) {
    try {
        if (debugLog( General )) debugRecord() << "std::filesystem::create_directories( " << directoryPath << " )" << record;
        create_directories( directoryPath );
        if (debugLog( General )) debugRecord() << "std::ifstream( " << (directoryPath / "nonExisting.txt").c_str() << " )" << record;
        ifstream nefile( directoryPath / "nonExisting.txt" );
        nefile.close();
        if (debugLog( General )) debugRecord() << "std::ifstream( " << (directoryPath / "moreJunk.txt").c_str() << " )" << record;
        ifstream ifile( directoryPath / "moreJunk.txt" );
        ifile.close();
        if (debugLog( General )) debugRecord() << "std::ofstream( " << (directoryPath / "junk.txt").c_str() << " )" << record;
        ofstream file( directoryPath / "junk.txt" );
        file << "Hello world!\n";
        file.close();
        this_thread::sleep_for(chrono::milliseconds(rand() % 17));
        if (debugLog( General )) debugRecord() << "std::ofstream( " << (directoryPath / "moreJunk.txt").c_str() << " )" << record;
        ofstream anotherFile( directoryPath / "moreJunk.txt" );
        anotherFile << "Hello again!\n";
        anotherFile.close();
        if (debugLog( General )) debugRecord() << "Determine canonical path of " << (directoryPath / "morejunk.txt").c_str() << record;
        auto canon = canonical( directoryPath / "morejunk.txt" );
        if (debugLog( General )) debugRecord() << "Canonical path is " << canon.c_str() << record;
        CopyFileW( (directoryPath / "moreJunk.txt").c_str(), (directoryPath / "evenMoreJunk.txt").c_str(), false );
        if (debugLog( General )) debugRecord() << "std::filesystem::remove( " << (directoryPath / "junk.txt").c_str() << " )" << record;
        remove( directoryPath / "junk.txt" );
        if (debugLog( General )) debugRecord() << "std::filesystem::rename( " << (directoryPath / "moreJunk.txt").c_str() << ", " << (directoryPath / "yetMorejunk.txt").c_str() << " )" << record;
        rename( directoryPath / "moreJunk.txt", directoryPath / "yetMoreJunk.txt" );
        if (debugLog( General )) debugRecord() << "std::filesystem::remove_all( " << directoryPath << " )" << record;
        error_code ec;
        remove_all( directoryPath, ec );
        if (debugLog( General )) debugRecord() << "std::filesystem::create_directories( " << directoryPath << " )" << record;
        create_directories( directoryPath );
        file = ofstream( directoryPath / "junk.txt" );
        file << "Hello world!\n";
        file.close();
    }
    catch (exception const& exception) {
        if (debugLog( General )) debugRecord() << "Exception  " << exception.what() << " in worker!" << record;
    }
    catch (...) {
        if (debugLog( General )) debugRecord() << "Exception[ " << GetLastError() << " ] in worker!" << record;
    }
}

void doFileAccess() {
    try {
        auto session( Session::current() );
        if (remoteProcess) {
            stringstream command;
            command
                << remoteTestFile
                << " " 
                << session->id()
                << " " 
                << threads
                << " "
                << session->directory().generic_string();
            if (debugLog( General )) debugRecord() << "Executing " << command.str().c_str() << record;
            // auto exitCode = system( command.str().c_str() );
            executeCommand( command.str().c_str() );
        }
        path sessionDir( session->directory() / uniqueName( L"Session", session->id() ) );
        if (1 < threads) {
            vector<thread> workerThreads;
            for (int i = 0; i < threads; ++i) {
                wstringstream subdir;
                subdir << L"fileAccessTest" << i;
                workerThreads.push_back( thread( worker, sessionDir / subdir.str() ) );
            }
            for (int i = 0; i < threads; ++i) workerThreads[ i ].join();
        } else {
            worker( sessionDir / L"fileAccessTest" );
        }
    }
    catch (exception const& exception) {
        if (debugLog( General )) debugRecord() << "Exception  " << exception.what() << " in doFileAccess!" << record;
    }
    catch (...) {
        if (debugLog( General )) debugRecord() << "Exception[ " << GetLastError() << " ] in doFileAccess!" << record;
    }
}

void doMonitoredFileAccess( int thread, int iteration ) {
    try {
        path temp( temp_directory_path() );
        auto aspects = MonitorLogAspects( General | RegisteredFunction | PatchedFunction | PatchExecution | FileAccesses );
        startMonitoring( temp, aspects );
        auto session( Session::current() );
        doFileAccess();
        MonitorEvents events;
        stopMonitoring( &events );
        // Log (all) events for this session...
        wstringstream logName;
        logName << L"TestProgramOutput";
        if ( 0 < thread ) logName << L"_" << thread;
        if ( 0 < iteration ) logName << L"_" << iteration;
        logName << L".txt";
        LogFile output( temp / logName.str() );
        for ( auto access : events ) {
            wstring fileName( access.first.generic_wstring() );
            FileAccess fileAccess( access.second );
            output()
                << fileName
                << L" [ " << fileAccess.writeTime() << L" ] "
                << fileAccessModeToString( fileAccess.modes() ) << (fileAccess.failures() ? " (one or more failures)" : "" )
                << " : " << fileAccessModeToString( fileAccess.mode() ) << (fileAccess.success() ? "" : " failed" ) << record;
        }
    }
    catch (exception const& exception) {
        cout << "Exception  " << exception.what() << " in doMonitoredFileAccess!" << endl;
    }
    catch (...) {
        cout << "Exception[ " << GetLastError() << " ] in doMonitoredFileAccess!" << endl;
    }
}

void doMultipleMonitoredFileAccess( int thread ) {
    if ( 1 < iterations ) for (int i = 0; i < iterations; ++i) doMonitoredFileAccess( thread, (i + 1) );
    else doMonitoredFileAccess( thread, 0 );
}

bool condition( const string& argument ) {
    if ((argument == "t") || (argument == "T")) return true;
    if ((argument == "true") || (argument == "TRUE")) return true;
    return false;
}

int main( int argc, char* argv[] ) {
    try {
        if (4 < argc) { remoteProcess = condition( argv[ 4 ] ); }
        if (3 < argc) { iterations = atoi( argv[ 3 ] ); }
        if (2 < argc) { threads = atoi( argv[ 2 ] ); }
        if (threads <= 1) threads = 1;
        if (1 < argc) { sessions = atoi( argv[ 1 ] ); }
        if (sessions <= 1) sessions = 1;
        if (1 < sessions) {
            vector<thread> sessionThreads;
            for (int i = 0; i < sessions; ++i) sessionThreads.push_back( thread( doMultipleMonitoredFileAccess, (i + 1) ) );
            for (int i = 0; i < sessions; ++i) sessionThreads[ i ].join();
        } else {
            doMultipleMonitoredFileAccess( 0 );
        }
    }
    catch (exception const& exception) {
        cout << "Exception  " << exception.what() << " in main!" << endl;
    }
    catch (...) {
        cout << "Exception[ " << GetLastError() << " ] in main!" << endl;
    }
};

