#include "Session.h"

#include <set>
#include <map>
#include <mutex>
#include <iostream>
#include <windows.h>

using namespace std;
using namespace filesystem;

namespace AccessMonitor {

    namespace {
    
        unsigned int tlsSessionIndex = -1;
        mutex sessionMutex;
        map<SessionID,Session*> sessions;
        set<SessionID> terminatedSessions;
        vector<SessionID> freeSessions;
        SessionID nextSession = 1;

        // Name of memory map used to pass session context to remote process.
        inline string sessionInfoMapName( const ProcessID process ) {
            stringstream unique;
            unique << "RemoteProcessSessionData" << "_" << process;
            return unique.str();
        }
        
        // Single session associated with all threads in a remote process.
        Session* remoteSession = nullptr;

        struct ThreadContext {
            SessionID       session;
            MonitorAccess   fileGuard;
            MonitorAccess   processGuard;
            ThreadContext() = delete;
            ThreadContext( SessionID id, LogFile* eventLog = nullptr, LogFile* debugLog = nullptr ) : session( id ) {}
        };
        inline ThreadContext* threadContext() { return( static_cast<ThreadContext*>(TlsGetValue(tlsSessionIndex)) ); }

    }

    Session::Session( const std::filesystem::path& directory, const LogAspects aspects ) : events( nullptr ), debug( nullptr ) {
        static const char* signature = "Session::Session( const std::filesystem::path& dir, const LogAspects dasp = 0 )";
        auto ctx( threadContext() );
        if (ctx != nullptr) throw runtime_error( string( signature ) + " - Thread already active on a session!" );
        // Create new session in root process...
        const lock_guard<mutex> lock( sessionMutex );
        if (remoteSession != nullptr) throw runtime_error( string( signature ) + " - Invalid call in remote session!" );
        if (tlsSessionIndex == -1) tlsSessionIndex = TlsAlloc();
        if (tlsSessionIndex == TLS_OUT_OF_INDEXES) throw runtime_error( string( signature ) + " - Could not allocate thread local storage index!" );
        SessionID newID;
        if (0 < freeSessions.size()) {
            newID = freeSessions.back();
            freeSessions.pop_back();
        } else {
            newID = nextSession++;
        }
        context.directory = directory;
        context.session = newID;
        context.aspects = aspects;
        sessions.insert( { newID, this } );
        addThread();
    }
    Session::Session( const SessionContext& ctx ) : context( ctx ), events( nullptr ), debug( nullptr ) {
        static const char* signature = "Session::Session( const SessionContext& ctx )";
        const lock_guard<mutex> lock( sessionMutex );
        if (0 < count()) throw runtime_error( string( signature ) + " - One or more session already active!" );
        tlsSessionIndex = TlsAlloc();
        if (tlsSessionIndex == TLS_OUT_OF_INDEXES) throw runtime_error( string( signature ) + " - Could not allocate thread local storage index!" );
        sessions.insert( { context.session, this } );
        remoteSession = this;
        addThread();
    }
    Session::~Session() {
        const lock_guard<mutex> lock( sessionMutex );
        SessionID id = context.session;
        if (sessions.count( id ) == 0) _terminate();
        terminatedSessions.erase( id );
        freeSessions.push_back( id );
        if (count() == 0) {
            // Removing last active session...
            TlsFree( tlsSessionIndex );
            tlsSessionIndex = -1;
        }
    }
    void Session::_terminate() {
        SessionID id = context.session;
        sessions.erase( id );
        terminatedSessions.insert( id );
        if (this == remoteSession) remoteSession = nullptr;
        delete events;
        events = nullptr;
        if (debug != nullptr) {
            delete debug;
            debug = nullptr;
        }
    }
    void Session::terminate() {
        static const char* signature = "void Session::terminate()";
        const lock_guard<mutex> lock( sessionMutex );
        if (sessions.count( context.session ) == 0) throw runtime_error( string(signature) + " - Session already terminated!" );
        _terminate();
    }
    Session* Session::current() {
        static const char* signature = "Session* Session::current()";
        auto context( threadContext() );
        if (context == nullptr) {
            if (remoteSession == nullptr) return nullptr;
            // Executing in a thread that was not explicitly created in a (remote) session (e.g., main thread).
            remoteSession->addThread();
            return remoteSession;
        }
        auto session( sessions.find( context->session ) );
        if (session == sessions.end()) return nullptr;
        return( session->second );
    }
    int Session::count() { return sessions.size(); }
    const std::filesystem::path& Session::directory() const { return context.directory; }
    SessionID Session::id() const { return context.session; }
    LogAspects Session::aspects() const { return context.aspects; }
    void Session::addThread() const {
        static const char* signature = "void Session::addThread() const";
        auto context( threadContext() );
        if (context != nullptr) throw runtime_error( string( signature ) + " - Thread already active on a session!" );
        TlsSetValue( tlsSessionIndex, new ThreadContext( id() ) );
    };
    void Session::removeThread() const {
        static const char* signature = "void Session::removeThread() const";
        auto context( threadContext() );
        if (context == nullptr) throw runtime_error( string( signature ) + " - Thread not active on a session!" );
        if (context->session != id()) throw runtime_error( string( signature ) + " - Invalid session ID!" );
        delete context;
        TlsSetValue( tlsSessionIndex, nullptr );

    }
    void Session::eventLog( LogFile* file ) {
        static const char* signature = "void Session::debugLog( LogFile* file )";
        if (events != nullptr) throw runtime_error( string( signature ) + " - Event log already defined for session!" );
        events = file;
    }
    LogFile* Session::eventLog() const { return events; }
    void Session::debugLog( LogFile* file ) {
        static const char* signature = "void Session::debugLog( LogFile* file )";
        if (debug != nullptr) throw runtime_error( string( signature ) + " - Debug log already defined for session!" );
        debug = file;
    }
    LogFile* Session::debugLog() const { return debug; }
    MonitorAccess* Session::fileAccess() {
        auto context( threadContext() );
        if (context == nullptr) {
            if (Session::current() == nullptr) return nullptr;
            context = threadContext();
        }
        return &context->fileGuard;
    }
    MonitorAccess* Session::processAccess() {
        auto context( threadContext() );
        if (context == nullptr) {
            if (Session::current() == nullptr) return nullptr;
            context = threadContext();
        }
        return &context->processGuard;
    }

    // Session context passeed to spawned process to enable creating (opening) a session in a remote process.
    // See recordSessionContext and retrieveSessionContext..
    struct SessionConextData {
        SessionID   session;                // The session in which the process was spawned
        LogAspects  aspects;           // The debugging aspects to be applied in the spawned process
        char        directory[ MAX_PATH ];  // The directory in which monitor data is stored
    };

    void* Session::recordContext( const ProcessID process ) const {
        const char* signature( "void recordSessionContext( const path& dir, const ProcessID process, const SessionID session, ThreadID thread )" );
        auto map = CreateFileMappingA( INVALID_HANDLE_VALUE, nullptr,   PAGE_READWRITE, 0, sizeof( SessionConextData ), sessionInfoMapName( process ).c_str() );
        auto error = GetLastError();
        if (map == nullptr) throw runtime_error( string( signature ) + " - Could not create file mapping!" );
        auto address = MapViewOfFile( map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof( SessionConextData ) );
        if (address == nullptr) throw runtime_error( string( signature ) + " - Could not open mapping!" );
        auto data = static_cast<SessionConextData*>( address );
        data->session = context.session;
        data->aspects = context.aspects;
        const auto dirString = context.directory.generic_string();
        memcpy( data->directory, dirString.c_str(), dirString.size() );
        UnmapViewOfFile( address );
        return map;
    }
    void Session::releaseContext( void* context ) { CloseHandle( context ); }
    const SessionContext Session::retrieveContext( const ProcessID process ) {
        const char* signature( "const SessionContext Session::retreive( const ProcessID process )" );
        auto map = OpenFileMappingA( FILE_MAP_ALL_ACCESS, false, sessionInfoMapName( process ).c_str() );
        if (map == nullptr) throw runtime_error( string( signature ) + " - Could not create file mapping!" );
        void* address = MapViewOfFile( map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof( SessionConextData ) );
        if (address == nullptr) throw runtime_error( string( signature ) + " - Could not open mapping!" );
        auto data = static_cast<const SessionConextData*>( address );
        SessionContext context( data->directory, data->session, data->aspects );
        UnmapViewOfFile( address );
        CloseHandle( map );
        return( context );
    }

} // namespace AccessMonitor