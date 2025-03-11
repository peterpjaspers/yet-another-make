#include "../Session.h"
#include "../FileNaming.h"
#include "../MonitorLogging.h"

#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <windows.h>

// Simulate DLL load in remote process to enable debug...

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

std::wstring getPatchDLLFile()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::filesystem::path modulePath(path);
    modulePath = modulePath.parent_path() / "accessMonitorDll.dll";
    return modulePath.wstring();
}

//const std::wstring patchDLLFile( L"C:/Users/philv/Code/yam/yet-another-make/accessMonitor/dll/accessMonitor64.dll" );
//const std::wstring patchDLLFile( L"C:/Users/peter/Documents/yam/github/main/x64/Debug/accessMonitorDll.dll" );

mutex suspend;

void doFileAccess( const path directoryPath ) {
    lock_guard<mutex> lock( suspend );
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
    SessionID id( 1 );
    // Create thread to act as main thread, it will wait on the suspend mutex pending DLL load.
    suspend.lock();
    path temp( temp_directory_path() );
    std::thread worker( doFileAccess, temp / uniqueName( L"DLLSession", id ) );
    // Manually create a (simulated remote) session and its data directory...
    SessionContext context( { temp, id, (RegisteredFunction | PatchedFunction | PatchExecution | FileAccesses | WriteTime) } );
    auto session( Session::start( context ) );
    const path sessionData( sessionDataPath( temp, session->id() ) );
    if (exists( sessionData )) {
        error_code ec;
        remove_all( sessionData, ec );
    }
    create_directory( sessionData );
    auto process = CurrentProcessID();
    auto thread = CurrentThreadID();
    auto patched = AccessEvent( "ProcessPatched", process );
    auto data( session->recordContext( process ) );
    auto patchDLLFile = getPatchDLLFile();
    HMODULE library = LoadLibraryW( patchDLLFile.c_str() );
    auto error = GetLastError();
    if (error != 0) cout << "LoadLibraryW failed with error " << error << endl;
    EventWait( patched );
    Session::releaseContext( data );
    ReleaseEvent( patched );
    // Now allow the "main" thread to continue
    suspend.unlock();
    worker.join();
    session->stop();
}
