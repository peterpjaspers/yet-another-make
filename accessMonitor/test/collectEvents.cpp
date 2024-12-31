#include "../FileNaming.h"
#include "../LogFile.h"
#include "../FileAccess.h"

#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <map>
#include <cstdlib>

using namespace AccessMonitor;
using namespace std;
using namespace std::filesystem;

typedef unsigned long SessionID;
typedef std::map<std::wstring,FileAccess> MonitorEvents;

void collectMonitorEvents( const path& directory, const SessionID session, MonitorEvents& collected ) {
    const path sessionData( sessionDataPath( directory, session ) );
    for (auto const& entry : directory_iterator( sessionData )) {
        auto eventFileName( entry.path().generic_string() );
        wifstream eventFile( eventFileName );
        wstring filePath;
        FileTime lastWriteTime;
        wstring modeString;
        bool success;
        while (!eventFile.eof()) {
            eventFile >> ws >> filePath >> ws;
            from_stream( eventFile, L"[ %Y-%m-%d %H:%M:%10S ]", lastWriteTime );
            eventFile >> ws >> modeString >> ws >> success;
            auto mode( stringToFileAccessMode( modeString ) );
            if (eventFile.good()) {
                if (0 < collected.count( filePath )) {
                    collected[ filePath ].mode( mode, lastWriteTime, success );                            
                } else {
                    collected[ filePath ] = FileAccess( mode, lastWriteTime, success );
                }
            } else break; // Presumably file is corrupt, ignore further content...
        }
        eventFile.close();
    }
}

int main( int argc, char* argv[] ) {
    int sessions = 1;
    if (1 < argc) { sessions = atoi( argv[ 1 ] ); }
    path directory( temp_directory_path() );
    for (int session = 1; session <= sessions; session++ ) {
        MonitorEvents events;
        collectMonitorEvents( directory, session, events );
        // Log (all) events for this session...
        LogFile output( directory / uniqueName( L"TestProgramOutput", session, L"txt" )  );
        for ( auto access : events ) {
            wstring fileName( access.first );
            FileAccess fileAccess( access.second );
            output()
                << fileName
                << L" [ " << fileAccess.writeTime() << L" ] "
                << fileAccessModeToString( fileAccess.modes() ) << (fileAccess.failures() ? " (one or more failures)" : "" )
                << " : " << fileAccessModeToString( fileAccess.mode() ) << (fileAccess.success() ? "" : " failed" ) << record;
        }
    }
};

