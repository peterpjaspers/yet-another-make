#include "Process.h"

#include <Windows.h>
#include <string>
#include <sstream>

using namespace std;

// Encapsulate OS specific process control
// ToDo: Implenent for Linux and MacOS, current implementation is Windows only...

namespace AccessMonitor {

    ProcessID CurrentProcessID() { return static_cast<ProcessID>( GetCurrentProcessId() ); }
    ProcessID GetProcessID( unsigned long id ) { return static_cast<ProcessID>( id ); }
    ThreadID CurrentThreadID() { return static_cast<ThreadID>( GetCurrentThreadId() ); }
    ThreadID GetThreadID( unsigned long id ) { return static_cast<ThreadID>( id ); }

    string uniqueEventName( const string& tag, ProcessID pid ) {
        stringstream name;
        name << "Global\\" << tag << "_Event_" << hex << pid;
        return name.str();
    }

    EventID AccessEvent( const string& tag, const ProcessID pid ) {
        HANDLE handle = CreateEventA( nullptr, false, false, uniqueEventName( tag, pid ).c_str() );
        return static_cast<EventID>( handle );
    }
    void ReleaseEvent( EventID event ) { CloseHandle( event ); }

    bool EventWait( EventID event, unsigned long milliseconds ) {
        DWORD started = WaitForSingleObject( event, milliseconds );
        if (started == WAIT_OBJECT_0) return true;
        return false;
    }
    void EventSignal( EventID event ) { SetEvent( event ); }


}
