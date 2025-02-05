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
        unsigned long SessionIDMask = (Session::MaxSessionID - 1);
        unsigned long TerminatedBit = (1 << (Session::SessionIDBits + 1));
        unsigned long FreeBit = (1 << (Session::SessionIDBits + 2));
        mutex sessionMutex;
        Session sessions[ Session::MaxSessionID ];
        unsigned long usedCount( Session::initialize() );
        int activeCount = 0;
        vector<SessionID> freeSessions;
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
            MonitorAccess   fileAccess;
            MonitorAccess   processAccess;
            ThreadContext() = delete;
            ThreadContext( SessionID id, LogFile* eventLog = nullptr, LogFile* debugLog = nullptr ) : session( id ) {}
        };
        inline ThreadContext* threadContext() {
            auto err = GetLastError();
            auto context = (static_cast<ThreadContext*>(TlsGetValue(tlsSessionIndex)));
            SetLastError(err);
            return context;
        }
        Session* currentSession( ThreadContext* context ) {
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

        ThreadContext* getThreadContext() {
            auto context( threadContext() );
            if (context == nullptr) {
                if (currentSession( context ) == nullptr) return nullptr;
                context = threadContext();
            }
            return context;
        }

    }

    SessionID Session::initialize() {
        for (SessionID id = 1; id <= Session::MaxSessionID; ++id) {
            auto session = &sessions[ id - 1 ];
            session->context.session = (id | FreeBit);
        }
        return 0;
    }
    // Create new session in root process...
    Session* Session::start( const std::filesystem::path& directory, const LogAspects aspects ) {
        static const char* signature = "Session* Session::start( const std::filesystem::path& dir, const LogAspects dasp = 0 )";
        auto ctx( threadContext() );
        if (ctx != nullptr) throw runtime_error( string( signature ) + " - Thread already active on a session!" );
        const lock_guard<mutex> lock( sessionMutex );
        if (remoteSession != nullptr) throw runtime_error( string( signature ) + " - Invalid call in remote session!" );
        if (tlsSessionIndex == -1) tlsSessionIndex = TlsAlloc();
        if (tlsSessionIndex == TLS_OUT_OF_INDEXES) throw runtime_error( string( signature ) + " - Could not allocate thread local storage index!" );
        SessionID newID;
        if ( freeSessions.empty() ) {
            if ( usedCount == MaxSessionID ) throw runtime_error( string( signature ) + " - Maximum number of sessions exceeded!" );
            newID = ++usedCount;
        } else {
            newID = freeSessions.back();
            freeSessions.pop_back();
        }
        auto session( Session::session( newID ) );
        session->context.session &= ~FreeBit;
        activeCount += 1;
        session->context.directory = directory;
        session->context.aspects = aspects;
        session->addThread();
        return( session );
    }
    // Extend session in remote process...
    Session* Session::start( const SessionContext& ctx ) {
        static const char* signature = "Session* Session::start( const SessionContext& ctx )";
        const lock_guard<mutex> lock( sessionMutex );
        if (0 < activeCount) throw runtime_error( string( signature ) + " - Cannot extend session while sessions are active!" );
        tlsSessionIndex = TlsAlloc();
        if (tlsSessionIndex == TLS_OUT_OF_INDEXES) throw runtime_error( string( signature ) + " - Could not allocate thread local storage index!" );
        auto session( &sessions[ ctx.session - 1 ] );
        session->context.session &= ~FreeBit;
        activeCount += 1;
        session->context.directory = ctx.directory;
        session->context.aspects = ctx.aspects;
        remoteSession = session;
        session->addThread();
        return( session );
    }
    void Session::_terminate() {
        context.session |= TerminatedBit;
        activeCount -= 1;
        delete events; // Closes event log file
        events = nullptr;
        if (debug != nullptr) {
            delete debug; // Closes debug log file
            debug = nullptr;
        }
        if (this == remoteSession) remoteSession = nullptr;
    }
    void Session::terminate() {
        static const char* signature = "void Session::terminate()";
        const lock_guard<mutex> lock( sessionMutex );
        if (terminated()) throw runtime_error( string(signature) + " - Session already terminated!" );
        _terminate();
    }
    inline bool Session::terminated() const { return( (context.session & TerminatedBit ) == TerminatedBit ); }
    inline bool Session::free() const { return( (context.session & FreeBit ) == FreeBit ); }
    void Session::stop() {
        const lock_guard<mutex> lock( sessionMutex );
        if (!terminated()) _terminate();
        context.session &= ~TerminatedBit;
        context.session |= FreeBit;
        freeSessions.push_back( id() );
        removeThread();
        if (freeSessions.size() == Session::MaxSessionID) {
            // Stopped last active session...
            TlsFree( tlsSessionIndex );
            tlsSessionIndex = -1;
        }
    }
    Session* Session::session( const SessionID id ) {
        static const char* signature = "Session* Session::session( const SessionID id )";
        if ((remoteSession == nullptr) && (usedCount < id)) throw runtime_error( string(signature) + " - Invalid session ID!" );
        return &sessions[ id - 1 ];
    }
    Session* Session::current() { return currentSession( threadContext() ); }
    int Session::count() { return ( activeCount ); }
    const std::filesystem::path& Session::directory() const { return( context.directory ); }
    SessionID Session::id() const { return( context.session & SessionIDMask ); }
    void Session::addThread() const {
        static const char* signature = "void Session::addThread() const";
        auto context( threadContext() );
        if (context != nullptr) throw runtime_error( string( signature ) + " - Thread already active on a session!" );
        if (terminated()) throw runtime_error( string( signature ) + " - Adding thread to terminated session!" );
        if (free()) throw runtime_error( string( signature ) + " - Adding thread to freed session!" );
        TlsSetValue( tlsSessionIndex, new ThreadContext( id() ) );
    };
    void Session::removeThread() const {
        static const char* signature = "void Session::removeThread() const";
        auto context( threadContext() );
        if (context == nullptr) throw runtime_error( string( signature ) + " - Thread not active on a session!" );
        if (context->session != id()) throw runtime_error( string( signature ) + " - Invalid session ID!" );
        delete context;
        if (debug != nullptr) debug->removeThread();
        if (events != nullptr) events->removeThread();
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
    MonitorAccess* Session::monitorFileAccess() {
        auto context( getThreadContext() );
        if (context == nullptr) return nullptr;
        return &context->fileAccess;
    }
    MonitorAccess* Session::monitorProcessAccess() {
        auto context( getThreadContext() );
        if (context == nullptr) return nullptr;
        return &context->processAccess;
    }

    // Session context passeed to spawned process to enable creating (opening) a session in a remote process.
    // See recordSessionContext and retrieveSessionContext..
    struct SessionConextData {
        SessionID   session;                // The session in which the process was spawned
        LogAspects  aspects;                // The debugging aspects to be applied in the spawned process
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