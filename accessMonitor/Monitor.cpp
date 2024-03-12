#include "Monitor.h"
#include "MonitorLogging.h"
#include "Process.h"
#include "FileNaming.h"
#include "MonitorFiles.h"
#include "MonitorThreadsAndProcesses.h"
#include "Patch.h"

#include <windows.h>
#include <fstream>
#include <filesystem>
#include <set>
#include <mutex>

using namespace std;
using namespace std::filesystem;

namespace AccessMonitor {

    namespace {

        mutex monitorMutex;

        void patchProcess() {
            registerFileAccess();
            registerProcessesAndThreads();
            patch();
        }
        void unpatchProcess() {
            unpatch();
            unregisterFileAccess();
            unregisterProcessCreation();
        }

        void createSessionData( const SessionID session ) {
            static const char* signature = "void createSessionData( const SessionID session )";
            const path sessionData( sessionDataPath( session ) );
            if (!create_directory( sessionData )) throw string( signature ) + " - Creating existing session data directory!";
        }

        // Delete all data data associated with a session.
        // Fails if session directory exists
        void removeSessionData( const SessionID session ) {
            static const char* signature = "void removeSessionData( const SessionID session )";
            const path sessionData( sessionDataPath( session ) );
            if (create_directory( sessionData )) throw string( signature ) + " - Session data directory does not exist!";
            for ( auto entry : directory_iterator( sessionData ) ) {
                auto file = entry.path();
                if ( is_regular_file( file ) ) remove( file );
            }
            remove( sessionData );
            remove( sessionInfoPath( CurrentProcessID() ) );
        }

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
        monitorLog.enable( PatchedFunction | ParseLibrary | PatchExecution | FileAccesses );
        SessionID session;
        {
            const lock_guard<mutex> lock( monitorMutex ); 
            if (SessionCount() == 0) patchProcess();
            session = CreateSession();
        }
        monitorLog() << "Start monitoring session " << session << "..." << record;
        SetSessionState( SessionUpdating );
        createSessionData( session );
        createEventLog( session );
        SetSessionState( SessionActive );
    }

    MonitorEvents stopMonitoring() {
        auto session = ThreadSessionID( CurrentThreadID() );
        monitorLog() << "Stop monitoring session " << session << "..." << record;
        {
            const lock_guard<mutex> lock( monitorMutex ); 
            RemoveSession();
            if (SessionCount() == 0) unpatchProcess();
        }
        closeEventLog( session );
        auto events = collectMonitorEvents( session );
        removeSessionData( session );
        return events;
    }

} // namespace AccessMonitor

