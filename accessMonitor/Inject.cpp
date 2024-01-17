#include "Inject.h"
#include "Process.h"

#include <windows.h>
#include <sstream>

using namespace std;

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
    void inject( PID pid, const wstring& library ) {
        const char* signature = "void inject( PID pid, const wstring& library )";
        HANDLE process = OpenProcess( PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid );
        if (process != nullptr) {
            size_t fileNameSize = ((library.size() + 1) * sizeof(wchar_t));
            wchar_t* fileName = static_cast<wchar_t*>( VirtualAllocEx( process, NULL, fileNameSize, MEM_COMMIT, PAGE_READWRITE ) );
            if (fileName != nullptr) {
                if (WriteProcessMemory( process, fileName, library.c_str(), fileNameSize, NULL)) {
                    HMODULE module = GetModuleHandleW( L"Kernel32" );
                    if (module != nullptr) {
                        PTHREAD_START_ROUTINE function = PTHREAD_START_ROUTINE( GetProcAddress( module, "LoadLibraryW" ) );
                        if (function != nullptr) {
                            HANDLE thread = CreateRemoteThread( process, nullptr, 0, function, fileName, CREATE_SUSPENDED, nullptr );
                            if (thread != nullptr) {
                                if (!SetThreadPriority( thread, THREAD_PRIORITY_HIGHEST )) {
                                    throw exceptionText( signature, "Failed to set remote thread priority" );
                                }
                                if (!ResumeThread( thread )) {
                                    throw exceptionText( signature, "Failed to resume remote thread" );
                                }
                                CloseHandle( thread );
                            } else throw exceptionText( signature, "Failed to create remote thread" );
                        } else throw exceptionText( signature, "Failed to access LoadLibraryW function pointer" );
                    } else throw exceptionText( signature, "Failed to access Kernel32 module" );
                } else throw exceptionText( signature, "Failed to write to remote memory" );
            } else throw exceptionText( signature, "Failed to allocate remote memory" );
            CloseHandle( process );
        } else throw string( signature ) + "Failed to open target process";
    }

} // namespace accessMonitor
