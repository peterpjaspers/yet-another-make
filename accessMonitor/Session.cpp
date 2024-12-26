#include "Session.h"

#include <set>
#include <map>
#include <mutex>
#include <windows.h>

using namespace std;
using namespace filesystem;

namespace AccessMonitor {

    namespace {
    
        unsigned int tlsSessionIndex = -1;
        mutex sessionMutex;
        Session sessions[ Session::MaxSessionID ];
        SessionID maxID( Session::initialize() );
        set<SessionID> activeSessions;
        set<SessionID> terminatedSessions;
        set<SessionID> freeSessions;
        // Single session associated with all threads in a remote process.
        Session* remoteSession( nullptr );

        // Name of memory map used to pass session context to remote process.
        inline string sessionInfoMapName( const ProcessID process ) {
            stringstream unique;
            unique << "RemoteProcessSessionData" << "_" << process;
            return unique.str();
        }

        struct ThreadContext {
            SessionID       session;
            MonitorAccess   access;
            ThreadContext() = delete;
            ThreadContext( SessionID id, LogFile* eventLog = nullptr, LogFile* debugLog = nullptr ) : session( id ) {}
        };
        inline ThreadContext* threadContext() { return( static_cast<ThreadContext*>(TlsGetValue(tlsSessionIndex)) ); }

    }

    SessionID Session::initialize() {
        for (SessionID id = 1; id <= Session::MaxSessionID; ++id) {
            auto session( Session::session( id ) );
            session->context.session = id;
        }
        return 0;
    }
    Session* Session::start( const std::filesystem::path& directory, const LogAspects aspects ) {
        static const char* signature = "Session* Session::start( const std::filesystem::path& dir, const LogAspects dasp = 0 )";
        auto ctx( threadContext() );
        if (ctx != nullptr) throw runtime_error( string( signature ) + " - Thread already active on a session!" );
        // Create new session in root process...
        const lock_guard<mutex> lock( sessionMutex );
        if (remoteSession != nullptr) throw runtime_error( string( signature ) + " - Invalid call in remote session!" );
        if (tlsSessionIndex == -1) tlsSessionIndex = TlsAlloc();
        if (tlsSessionIndex == TLS_OUT_OF_INDEXES) throw runtime_error( string( signature ) + " - Could not allocate thread local storage index!" );
        SessionID newID;
        if ( freeSessions.empty() ) {
            if (maxID == MaxSessionID) throw runtime_error( string( signature ) + " - Maximum number of sessions exceeded!" );
            newID = ++maxID;
        } else {
            newID = *freeSessions.begin();
            freeSessions.erase( newID);
        }
        auto session( Session::session( newID ));
        activeSessions.insert( newID );
        session->context.directory = directory;
        session->context.aspects = aspects;
        session->addThread();
        return( session );
    }
    Session* Session::start( const SessionContext& ctx ) {
        static const char* signature = "Session* Session::start( const SessionContext& ctx )";
        const lock_guard<mutex> lock( sessionMutex );
        if (0 < count()) throw runtime_error( string( signature ) + " - Cannot extend session while sessions are active!" );
        tlsSessionIndex = TlsAlloc();
        if (tlsSessionIndex == TLS_OUT_OF_INDEXES) throw runtime_error( string( signature ) + " - Could not allocate thread local storage index!" );
        auto session( Session::session( ctx.session ) );
        activeSessions.insert( ctx.session );
        session->context = ctx;
        remoteSession = session;
        session->addThread();
        return( session );
    }
    void Session::_terminate() {
        activeSessions.erase( context.session );
        terminatedSessions.insert( context.session );
        if (this == remoteSession) remoteSession = nullptr;
        delete events; // Closes event log file
        events = nullptr;
        if (debug != nullptr) {
            delete debug; // Closes debug log file
            debug = nullptr;
        }
    }
    void Session::terminate() {
        static const char* signature = "void Session::terminate()";
        const lock_guard<mutex> lock( sessionMutex );
        if (terminated()) throw runtime_error( string(signature) + " - Session already terminated!" );
        _terminate();
    }
    inline bool Session::terminated() { return( terminatedSessions.contains( context.session ) ); }
    inline bool Session::free() { return( freeSessions.contains( context.session ) ); }
    void Session::stop() {
        const lock_guard<mutex> lock( sessionMutex );
        if (!terminated()) _terminate();
        terminatedSessions.erase( context.session );
        freeSessions.insert( context.session );
        removeThread();
        if (freeSessions.size() == Session::MaxSessionID) {
            // Stopped last active session...
            TlsFree( tlsSessionIndex );
            tlsSessionIndex = -1;
        }
    }
    inline Session* Session::session( const SessionID id ) { return &sessions[ id - 1 ]; }
    Session* Session::current() {
        static const char* signature = "Session* Session::current()";
        auto context( threadContext() );
        if (context == nullptr) {
            if (remoteSession == nullptr) return nullptr;
            // Executing in a thread that was not explicitly created in a (remote) session (e.g., main thread).
            remoteSession->addThread();
            return remoteSession;
        }
        auto session( Session::session( context->session ) );
        if (session->terminated()) return nullptr;
        if (session->free()) return nullptr;
        return( session );
    }
    inline int Session::count() { return ( activeSessions.size() ); }
    void Session::addThread() const {
        static const char* signature = "void Session::addThread() const";
        auto context( threadContext() );
        if (context != nullptr) throw runtime_error( string( signature ) + " - Thread already active on a session!" );
        TlsSetValue( tlsSessionIndex, new ThreadContext( this->context.session ) );
    };
    void Session::removeThread() const {
        static const char* signature = "void Session::removeThread() const";
        auto context( threadContext() );
        if (context == nullptr) throw runtime_error( string( signature ) + " - Thread not active on a session!" );
        if (context->session != this->context.session) throw runtime_error( string( signature ) + " - Invalid session ID!" );
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
    MonitorAccess* Session::monitorAccess() {
        if (Session::current() == nullptr) return nullptr;
        auto context( threadContext() );
        if (context == nullptr) return nullptr;
        return &context->access;
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
        data->session = id();
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