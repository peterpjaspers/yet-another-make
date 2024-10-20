#include "../Session.h"
#include "../FileNaming.h"
#include "../MonitorLogging.h"

#include <string>
#include <filesystem>
#include <fstream>
#include <thread>
#include <windows.h>

// Simulate DLL load in remote process to enable debug...

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

const std::wstring patchDLLFile( L"C:/Users/philv/Code/yam/yet-another-make/accessMonitor/dll/accessMonitor64.dll" );
// const std::wstring patchDLLFile( L"D:/Peter/github/yam/x64/Debug/dll/accessMonitorDLL.dll" );

void doFileAccess( const path directoryPath ) {
    create_directories( directoryPath );
    ofstream file( directoryPath / "junk.txt" );
    file << "Hello world!\n";
    file.close();
    this_thread::sleep_for(chrono::milliseconds(rand() % 17));;
    ofstream anotherFile( directoryPath / "moreJunk.txt" );
    anotherFile << "Hello again!\n";
    anotherFile.close();
    auto canon = canonical( directoryPath / "morejunk.txt" );
    CopyFileW( (directoryPath / "moreJunk.txt").c_str(), (directoryPath / "evenMoreJunk.txt").c_str(), false );
    remove( directoryPath / "junk.txt" );
    rename( directoryPath / "moreJunk.txt", directoryPath / "yetMoreJunk.txt" );
    error_code ec;
    remove_all( directoryPath, ec );
}

int main( int argc, char* argv[] ) {
    // Manually create a (simulated remote) session and its data directory...
    SessionID session = 1;
    const path sessionData( sessionDataPath( temp_directory_path(), session ) );
    if (exists( sessionData )) {
        error_code ec;
        remove_all( sessionData, ec );
    }
    create_directory( sessionData );
    auto process = CurrentProcessID();
    auto thread = CurrentThreadID();
    auto patched = AccessEvent( "ProcessPatched", process );
    auto data = recordSessionContext( process, session, (RegisteredFunction | PatchedFunction | PatchExecution | FileAccesses | WriteTime), temp_directory_path() );
    HMODULE library = LoadLibraryW( patchDLLFile.c_str() );
    auto error = GetLastError();
    if (error != 0) cout << "LoadLibraryW failed with error " << error << endl;
    EventWait( patched );
    releaseSessionContext( data );
    ReleaseEvent( patched );
    std::thread worker( doFileAccess, uniqueName( temp_directory_path() / L"DLLSession", session ) );
    worker.join();
}
