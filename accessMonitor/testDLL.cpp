#include "FileNaming.h"
#include "MonitorLogging.h"

#include <string>
#include <filesystem>
#include <fstream>
#include <windows.h>

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

const std::wstring patchDLLFile( L"C:\\Users\\philv\\Code\\yam\\yet-another-make\\accessMonitor\\accessMonitor.dll" );

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
    auto canon = canonical( directoryPath / "morejunk.txt" );
    CopyFileW( (directoryPath / "moreJunk.txt").c_str(), (directoryPath / "evenMoreJunk.txt").c_str(), false );
    remove( directoryPath / "junk.txt" );
    rename( directoryPath / "moreJunk.txt", directoryPath / "yetMoreJunk.txt" );
    error_code ec;
    remove_all( directoryPath, ec );
}

int main( int argc, char* argv[] ) {
    // Manually create a session data directory...
    auto session = 1;
    const path sessionData( sessionDataPath( session ) );
    if (exists( sessionData )) {
        error_code ec;
        remove_all( sessionData, ec );
    }
    create_directory( sessionData );
    auto process = CurrentProcessID();
    auto thread = CurrentThreadID();
    auto patched = AccessEvent( "ProcessPatched", session, process );
    auto exit = AccessEvent( "ExitProcess", session, process );
    recordSessionInfo( session, process, thread );
    HMODULE library = LoadLibraryW( patchDLLFile.c_str() );
    EventWait( patched );
    ReleaseEvent( patched );
    worker( uniqueName( L"Session", session ) );
}
