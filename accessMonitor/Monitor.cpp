#include "Monitor.h"
#include "Patch.h"
#include "Process.h"
#include "Session.h"
#include "FileNaming.h"
#include "MonitorFiles.h"
#include "MonitorThreadsAndProcesses.h"

#include <windows.h>
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
        void collectMonitorEvents( const path& directory, const SessionID session, MonitorEvents& collected ) {
            const path sessionData( sessionDataPath( directory, session ) );
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
                            auto mode = stringToMode( accessMode );
                            if ((mode & AccessDelete) != 0) access.mode = AccessDelete;
                            else if ((mode & AccessWrite) != 0) access.mode = AccessWrite;
                            else if (((mode & AccessRead) != 0) && ((access.mode & AccessDelete) == 0) && ((access.mode & AccessWrite) == 0)) access.mode = AccessRead;
                            access.modes |= mode;
                            if ((access.lastWriteTime < lastWriteTime) && ((mode & AccessRead) == 0)) access.lastWriteTime = lastWriteTime;
                        } else {
                            collected[ filePath ] = FileAccess( stringToMode( accessMode ), lastWriteTime );
                        }
                    } else break; // Presumably file is corrupt, ignore further content...
                }
                eventFile.close();
            }
            error_code ec;
            remove_all( sessionData, ec );
        }

        mutex monitorMutex;
    }

    void startMonitoring( const path& directory, const LogAspects aspects ) {
        const lock_guard<mutex> lock( monitorMutex );
        Session* session( new Session( directory, aspects ) );
        createSessionDirectory( directory, session->id() );
        create_directory( directory / dataDirectory() );
        session->debugLog( createDebugLog() );
        session->debugLog()->enable( aspects );
        debugRecord() << "Start monitoring session " << session->id() << "..." << record;
        session->eventLog( createEventLog() );
        if (Session::count() == 1) {
            registerFileAccess();
            registerProcessesAndThreads();
            patch();
        }
    }

    void startMonitoring( const SessionContext& context ) {
        const lock_guard<mutex> lock( monitorMutex );
        Session* session( new Session( context ) );
        createSessionDirectory( context.directory, session->id() );
        create_directory( context.directory / dataDirectory() );
        session->debugLog( createDebugLog() );
        session->debugLog()->enable( context.aspects );
        debugRecord() << "Extend monitoring session " << session->id() << "..." << record;
        session->eventLog( createEventLog() );
        if (Session::count() == 1) {
            registerFileAccess();
            registerProcessesAndThreads();
            patch();
        }
    }
    

    void stopMonitoring( MonitorEvents* events ) {
        const lock_guard<mutex> lock( monitorMutex );
        auto session( Session::current() );
        const auto id( session->id() );
        const auto directory( session->directory() );
        debugRecord() << "Stop monitoring session " << id << "..." << record;
        if (Session::count() == 1) {
            unpatch();
            unregisterProcessesAndThreads();
            unregisterFileAccess();
        }
        session->eventClose();
        session->debugClose();
        delete session;
        // Hold on to terminated session ID while collecting session results.
        if (events != nullptr) collectMonitorEvents( directory, id, *events );
        Session::release( id );
    }

    MonitorGuard::MonitorGuard( MonitorAccess* monitor ) : access( monitor ) {
        if (access != nullptr) {
            auto count( access->monitorCount++ );
            if (count == 0) access->errorCode = GetLastError();
        }
    }
    MonitorGuard::~MonitorGuard() {
        if (access != nullptr) {
            auto count( --access->monitorCount );
            if (count == 0) SetLastError( access->errorCode );
        }
    }

} // namespace AccessMonitor

