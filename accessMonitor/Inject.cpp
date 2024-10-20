#include "Inject.h"
#include "Session.h"

#include <windows.h>
#include <sstream>
#include <filesystem>

using namespace std;
using namespace std::filesystem;

namespace AccessMonitor {
    
    string exceptionText( const string& signature, const string& message ) {
        stringstream ss;
        ss << signature << " - " << message << "! [ " << GetLastError() << " ]";
        return ss.str();
    }

    // Injects a DLL library in a (remote) process via a remote thread in the target process
    // that excutes LoadLibrary as its (main) function. The DLLMain entry point of the library is
    // called as a result of loading the DLL.
    // Throws one of a number of failure specific exceptions.
    void inject( const string& library, ProcessID process, SessionID session, LogAspects aspects, const path& directory  ) {
        const char* signature = "void inject( const string& library, SessionID session, ProcessID process, ThreadID thread, const path& directory  )";
        HANDLE processHandle = OpenProcess( PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, (DWORD)process );
        if (processHandle != nullptr) {
            size_t fileNameSize = ((library.size() + 1) * sizeof(char));
            char* fileName = static_cast<char*>( VirtualAllocEx( processHandle, NULL, fileNameSize, MEM_COMMIT, PAGE_READWRITE ) );
            if (fileName != nullptr) {
                if (WriteProcessMemory( processHandle, fileName, library.c_str(), fileNameSize, NULL)) {
                    HMODULE module = GetModuleHandleW( L"Kernel32" );
                    if (module != nullptr) {
                        PTHREAD_START_ROUTINE function = reinterpret_cast<PTHREAD_START_ROUTINE>( GetProcAddress( module, "LoadLibraryA" ) );
                        if (function != nullptr) {
                            HANDLE threadHandle = CreateRemoteThread( processHandle, nullptr, 0, function, fileName, CREATE_SUSPENDED, nullptr );
                            // Communicate session ID, debug aspects and session (temp) directory to process via file mapping...
                            recordSessionContext( process, session, aspects, directory );
                            auto completed = AccessEvent( "ProcessPatched", process );
                            if (threadHandle != nullptr) {
                                if (ResumeThread( threadHandle ) < 0) throw exceptionText( signature, "Failed to resume remote thread" );
                                EventWait( completed );
                                CloseHandle( threadHandle );
                            } else throw exceptionText( signature, "Failed to create remote thread" );
                            ReleaseEvent( completed );
                        } else throw exceptionText( signature, "Failed to access LoadLibraryW function pointer" );
                    } else throw exceptionText( signature, "Failed to access Kernel32 module" );
                } else throw exceptionText( signature, "Failed to write to remote memory" );
            } else throw exceptionText( signature, "Failed to allocate remote memory" );
            CloseHandle( processHandle );
        } else throw exceptionText( signature, "Failed to open target process" );
    }

} // namespace accessMonitor
