#include "Monitor.h"
#include "MonitorFiles.h"
#include "Log.h"

#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

// Convert wide character string to ANSI character string
string narrow( const wstring& string ){
    static wstring_convert< codecvt_utf8_utf16< wstring::value_type >, wstring::value_type > utf16conv;
    return utf16conv.to_bytes( string );
}

void worker( const path directoryPath ) {
    // path externals = directoryPath / "externals.txt";
    // system( narrow((wstring( L"listDLLExternals.exe \"\" " ) + externals.wstring())).c_str() );
    create_directory( directoryPath, current_path() );
    ofstream file( directoryPath / "junk.txt" );
    file << "Hello world!\n";
    file.close();
    this_thread::sleep_for(chrono::milliseconds(100));;
    ofstream anotherFile( directoryPath / "moreJunk.txt" );
    anotherFile << "Hello again!\n";
    anotherFile.close();
    CopyFileW( (directoryPath / "moreJunk.txt").c_str(), (directoryPath / "evenMoreJunk.txt").c_str(), false );
    remove( directoryPath / "junk.txt" );
    rename( directoryPath / "moreJunk.txt", directoryPath / "yetMoreJunk.txt" );
    remove_all( directoryPath );
}

void doFileAccess( bool multithreaded ) {
    worker( path( "./fileAccessTest" ) );
    if (multithreaded) {
        auto t0 = jthread( worker, path( "./fileAccessTest0" ) );
        auto t1 = jthread( worker, path( "./fileAccessTest1" ) );
        auto t2 = jthread( worker, path( "./fileAccessTest2" ) );
        auto t3 = jthread( worker, path( "./fileAccessTest3" ) );
    }
}

int main() {
    bool multithreaded = true;
    try {
        enableLog( "testLog.txt", Terse );
        log() << "Performing file access without monitoring..." << endLine;
        doFileAccess( multithreaded );
        log() << "Start monitoring..." << endLine;
        startMonitoring();
        log() << "Performing file access with monitoring..." << endLine;
        doFileAccess( multithreaded );
        log() << "Stop monitoring..." << endLine;
        stopMonitoring();
        wofstream files( "./accessedFiles.txt" );
        streamAccessedFiles( files );
        files.close();
        log() << "Performing file access without monitoring..." << endLine;
        doFileAccess( multithreaded );
        log() << "Done..." << endLine;
    }
    catch ( string message ) {
        log() << widen( message ) << endLine;
    }
    catch (...) {
        log() << "Exception!" << endLine;
    }
    if (logging(None)) disableLog();
};

