#include "Process.h"

#include <sstream>
#include <windows.h>
#include <string>

using namespace std;

// Encapsulate OS specific process control
// ToDo: Implenent for Linux and MacOS, current implementation is Windows only...

namespace AccessMonitor {

    namespace {

        string uniqueEventName( const string& tag, const ProcessID process ) {
            stringstream name;
            name << "Global\\" << tag << "_Event_" << "_" << process;
            return name.str();
        }

    }

    ProcessID CurrentProcessID() { return static_cast<ProcessID>( GetCurrentProcessId() ); }
    ProcessID GetProcessID( unsigned int id ) { return static_cast<ProcessID>( id ); }

    ThreadID CurrentThreadID() { return static_cast<ThreadID>( GetCurrentThreadId() ); }
    ThreadID GetThreadID( unsigned int id ) { return static_cast<ThreadID>( id ); }

    EventID AccessEvent( const string& tag, const ProcessID process ) {
        HANDLE handle = CreateEventA( nullptr, false, false, uniqueEventName( tag, process ).c_str() );
        return static_cast<EventID>( handle );
    }
    void ReleaseEvent( EventID event ) { CloseHandle( event ); }

    bool EventWait( EventID event, unsigned long milliseconds ) {
        DWORD status = WaitForSingleObject( static_cast<HANDLE>( event ), milliseconds );
        if (status == WAIT_OBJECT_0) return true;
        return false;
    }
    bool EventWait( const std::string& tag, const ProcessID process, unsigned long milliseconds ) {
        auto event = AccessEvent( tag, process );
        auto signaled = EventWait( event, milliseconds );
        ReleaseEvent( event );
        return signaled;
    }

    void EventSignal( EventID event ) {
        SetEvent( static_cast<HANDLE>( event ) );
    }
    void EventSignal( const std::string& tag, const ProcessID process ) {
        auto event = AccessEvent( tag, process );
        EventSignal( event );
        ReleaseEvent( event );
    }

} // namespace AccessMonitor
