#include "Monitor.h"
#include "MonitorLogging.h"
#include "Process.h"
#include "Session.h"
#include "FileNaming.h"
#include "MonitorFiles.h"
#include "MonitorThreadsAndProcesses.h"
#include "PatchProcess.h"

#include <windows.h>
#include <fstream>
#include <filesystem>

using namespace std;
using namespace std::filesystem;

// ToDo: Clean-up Session ID files (created when injecting patch DLL in spawned process).

namespace AccessMonitor {

    namespace {

        // Create data directory for a session
        void createSessionDirectory( const SessionID session ) {
            const path sessionData( sessionDataPath( session ) );
            if (exists( sessionData )) {
                // Session directory already exists, presumably due to a previous session 
                // Remove all data session left behind...
                error_code ec;
                remove_all( sessionData, ec );
            }
            create_directory( sessionData );
        }

        // Collect events from monitor event files in a session.
        //
        // Multiple monitor events on the same file are collapsed to a
        // single monitor event.
        //
        // Event file access modes are collapsed in the following manner:
        //
        //         None   Read   Write  Delete
        // None    None   Read   Write  Delete
        // Read    Read   Read   Write  Delete
        // Write   Write  Write  Write  Write
        // Delete  Delete Delete Delete Delete
        //
        // where the top row indicates the previous collapsed mode state
        // and the left row the next event mode.
        //
        // Last write time is collapsed to the lastest last write time.
        //
        MonitorEvents collectMonitorEvents( const SessionID session ) {
            const path sessionData( sessionDataPath( session ) );
            MonitorEvents collected;
            for (auto const& entry : directory_iterator( sessionData )) {
                auto eventFile = wifstream( entry.path() );
                path filePath;
                FileTime lastWriteTime;
                wstring accessMode;
                while (!eventFile.eof()) {
                    eventFile >> ws >> filePath >> ws;
                    from_stream( eventFile, L"[ %Y-%m-%d %H:%M:%10S ]", lastWriteTime );
                    eventFile >> ws >> accessMode >> ws;
                    if (eventFile.good()) {
                        if (0 < collected.count( filePath )) {
                            auto& access = collected[ filePath ];
                            if (access.lastWriteTime < lastWriteTime) access.lastWriteTime = lastWriteTime;
                            auto mode = stringToMode( accessMode );
                            if ((mode & AccessDelete) != 0) access.mode = AccessDelete;
                            else if ((mode & AccessWrite) != 0) access.mode = AccessWrite;
                            else if (((mode & AccessRead) != 0) && ((access.mode & AccessDelete) == 0) && ((access.mode & AccessWrite) == 0)) access.mode = AccessRead;
                            access.modes |= mode;
                        } else {
                            collected[ filePath ] = FileAccess( stringToMode( accessMode ), lastWriteTime );
                        }
                    } else break; // Presumably file is corrupt, ignore further content...
                }
                eventFile.close();
            }
            return collected;
        }
    }

    void startMonitoring() {
        SessionID session = CreateSession();
        SessionDebugLog( createDebugLog() );
        debugLog().enable( PatchedFunction | ParseLibrary | PatchExecution | FileAccesses | WriteTime );
        debugRecord() << "Start monitoring session " << session << "..." << record;
        createSessionDirectory( session );
        SessionEventLog( createEventLog() );
        patchProcess();
    }

    MonitorEvents stopMonitoring() {
        auto session = CurrentSessionID();
        debugRecord() << "Stop monitoring session " << session << "..." << record;
        SessionEventLogClose();
        auto events = collectMonitorEvents( session );
        unpatchProcess();
        SessionDebugLogClose();
        RemoveSession();
        return events;
    }

} // namespace AccessMonitor

