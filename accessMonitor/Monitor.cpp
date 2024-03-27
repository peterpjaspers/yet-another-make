#include "Monitor.h"
#include "MonitorLogging.h"
#include "Process.h"
#include "FileNaming.h"
#include "MonitorFiles.h"
#include "MonitorThreadsAndProcesses.h"
#include "PatchProcess.h"

#include <windows.h>
#include <fstream>
#include <filesystem>
#include <set>
#include <mutex>

using namespace std;
using namespace std::filesystem;

namespace AccessMonitor {

    namespace {

        // Create data directory for a session
        void createSessionData( const SessionID session ) {
            static const char* signature = "void createSessionData( const SessionID session )";
            const path sessionData( sessionDataPath( session ) );
            if (exists( sessionData )) {
                // Session directory already exists, presumably due to a previous session 
                // with the same ID that terminated abnormally (e.g., crashed or was killed).
                // Remove all data session left behind...
                remove_all( sessionData );
            }
            create_directory( sessionData );
        }

        // Delete all data associated with a session.
        // Fails if session directory exists
        void removeSessionData( const SessionID session ) {
            static const char* signature = "void removeSessionData( const SessionID session )";
            const path sessionData( sessionDataPath( session ) );
            if (!exists( sessionData )) throw string( signature ) + " - Session data directory does not exist!";
            remove_all( sessionData );
            // Remove (remote) session ID files associated with a session
            // ToDo: This does not work, ID file has process ID of remote process, not current process
            remove( sessionInfoPath( CurrentProcessID() ) );
        }

        // ToDo: Access mode merge logic depending on event order
        // Collect events from monitor event files in a session
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
                            auto access = collected[ filePath ];
                            if (access.lastWriteTime < lastWriteTime) access.lastWriteTime = lastWriteTime;
                            access.mode |= stringToMode( accessMode );
                            collected[ filePath ] = access;
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

    // ToDo: Single definition/declaration of patchDLL.dll file name/path...
    // ToDo: rename patchDLL.dll to accessMonitor.dll

    void startMonitoring() {
        SessionID session = CreateSession();
        createDebugLog();
        debugLog().enable( PatchedFunction | ParseLibrary | PatchExecution | FileAccesses | WriteTime );
        debugRecord() << "Start monitoring session " << session << "..." << record;
        SetSessionState( SessionUpdating );
        createSessionData( session );
        createEventLog();
        SetSessionState( SessionActive );
        patchProcess();
    }

    MonitorEvents stopMonitoring() {
        auto session = CurrentSessionID();
        debugRecord() << "Stop monitoring session " << session << "..." << record;
        unpatchProcess();
        closeEventLog();
        closeDebugLog();
        auto events = collectMonitorEvents( session );
        RemoveSession();
        removeSessionData( session );
        return events;
    }

} // namespace AccessMonitor

