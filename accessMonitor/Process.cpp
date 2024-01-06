#include "Process.h"

#include <Windows.h>

namespace AccessMonitor {

    ProcessID CurrentProcessID() { return static_cast<ProcessID>( GetCurrentProcessId() ); }
    ProcessID GetProcessID( DWORD id ) { return static_cast<ProcessID>( id ); }

}
