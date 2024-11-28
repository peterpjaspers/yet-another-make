#include "Monitor.h"
#include "Patch.h"
#include "Process.h"
#include "Session.h"
#include "FileNaming.h"
#include "MonitorFiles.h"
#include "MonitorThreadsAndProcesses.h"

#include <windows.h>
#include <iostream>
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
        // and the left column the mode of the event being processed.
        //
        // Last write time is collapsed to the lastest last write time.
        //
        void collectMonitorEvents( const path& directory, const SessionID session, MonitorEvents& collected ) {
            const path sessionData( sessionDataPath( directory, session ) );
            for (auto const& entry : directory_iterator( sessionData )) {
                wifstream eventFile( entry.path().generic_string() );
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
#ifndef _DEBUG_ACCESS_MONITOR
            // Remove all event files when not debugging
            error_code ec;
            remove_all( sessionData, ec );
#endif
        }

        mutex monitorMutex;
    }

    void startMonitoring( const path& directory, const LogAspects aspects ) {
        const lock_guard<mutex> lock( monitorMutex );
        Session* session( new Session( directory, aspects ) );
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
            registerProcessesAndThreads();
            patch();
        }
    }

    void startMonitoring( const SessionContext& context ) {
        const lock_guard<mutex> lock( monitorMutex );
        Session* session( new Session( context ) );
        createSessionDirectory( context.directory, context.session );
        create_directory( context.directory / dataDirectory() );
        auto debugLogFile( createDebugLog( context.directory, context.session ) );
        if (debugLogFile != nullptr) {
            session->debugLog( debugLogFile );
            debugLogFile->enable( context.aspects );
        }
        if (debugLog( General )) debugRecord() << "Extend monitoring session " << session->id() << "..." << record;
        session->eventLog( createEventLog( context.directory, context.session ) );
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
        if (debugLog( General )) debugRecord() << "Stop monitoring session " << id << "..." << record;
        bool lastSession( Session::count() == 1 );
cout << "Terminate session..." << endl;
        session->terminate();
        if (lastSession) {
cout << "Unpatch..." << endl;
            unpatch();
            unregisterProcessesAndThreads();
            unregisterFileAccess();
        }
        // Hold on to terminated session ID while collecting session results.
cout << "Collect events..." << endl;
        if (events != nullptr) collectMonitorEvents( directory, id, *events );
cout << "Delete session..." << endl;
        delete session;
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

