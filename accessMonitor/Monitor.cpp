#include "Monitor.h"
#include "Patch.h"
#include "Process.h"
#include "Session.h"
#include "FileNaming.h"
#include "MonitorFiles.h"
#include "MonitorProcesses.h"

#include <fstream>
#include <filesystem>

using namespace std;
using namespace std::filesystem;

namespace AccessMonitor {

    namespace {

        // Create data directory for a session
        void createSessionDirectory( path const& directory, const SessionID session ) {
            const path sessionData( sessionDataPath(directory, session ) );
            if (exists( sessionData )) {
                // Session directory already exists, presumably due to a previous session 
                // Remove all data session left behind...
                error_code ec;
                remove_all( sessionData, ec );
            }
            create_directories( sessionData );
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
        // and the left column the mode of the event being processed.
        //
        // Last write time is collapsed to the lastest last write time.
        //
        void collectMonitorEvents( const path& directory, const SessionID session, MonitorEvents& collected, bool cleanUp ) {
            const path sessionData( sessionDataPath( directory, session ) );
            // for (auto const& entry : directory_iterator( sessionData )) {
            error_code error;
            for (auto const& entry : directory_iterator( sessionData, (directory_options::none | directory_options::skip_permission_denied), error )) {
                auto eventFile = wifstream(entry.path());
                path filePath;
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
            if (cleanUp) {
                // Remove all event files when not debugging
                error_code ec;
                remove_all( sessionData, ec );
            }
        }

        mutex monitorMutex;
    }

    void startMonitoring( const path& directory, const LogAspects aspects ) {
        const lock_guard<mutex> lock( monitorMutex );
        Session* session( Session::start( directory, aspects ) );
        createSessionDirectory( directory, session->id() );
        create_directory( directory / dataDirectory() );
        auto debugLogFile( createDebugLog( directory, session->id() ) );
        if (debugLogFile != nullptr) {
            session->debugLog( debugLogFile );
            debugLogFile->enable( aspects );
        }
        if (debugLog( General )) debugRecord() << "Start monitoring session " << session->id() << "..." << record;
        session->eventLog( createEventLog( directory, session->id() ) );
        if (Session::count() == 1) {
            registerFileAccess();
            registerProcessAccess();
            patch();
        }
    }

    void startMonitoring( const SessionContext& context ) {
        const lock_guard<mutex> lock( monitorMutex );
        Session* session( Session::start( context ) );
        auto debugLogFile( createDebugLog( context.directory, context.session ) );
        if (debugLogFile != nullptr) {
            session->debugLog( debugLogFile );
            debugLogFile->enable( context.aspects );
        }
        if (debugLog( General )) debugRecord() << "Extend monitoring session " << session->id() << "..." << record;
        session->eventLog( createEventLog( context.directory, context.session ) );
        if (Session::count() == 1) {
            registerFileAccess();
            registerProcessAccess();
            patch();
        }
    }
    
    void stopMonitoring( MonitorEvents* events ) {
        const lock_guard<mutex> lock( monitorMutex );
        auto session( Session::current() );
        const auto id( session->id() );
        const auto directory( session->directory() );
        if (debugLog( General )) debugRecord() << "Stop monitoring session " << id << "..." << record;
        if (Session::count() == 1) {
            unpatch();
            unregisterProcessAccess();
            unregisterFileAccess();
        }
        bool cleanUp( !debugLog( General ) ); // Clean-up event files when not debugging
        session->terminate();
        // Hold on to terminated session ID while collecting session results.
        monitorMutex.unlock();
        if (events != nullptr) collectMonitorEvents( directory, id, *events, cleanUp );
        monitorMutex.lock();
        session->stop();
    }

} // namespace AccessMonitor

