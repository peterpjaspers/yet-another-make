#include "Inject.h"
#include "Process.h"
#include "FileNaming.h"

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
    void inject( path const& sessionDirectory, SessionID session, ProcessID process, ThreadID thread, const wstring& library ) {
        const char* signature = "void inject( SessionID session, ProcessID process, const wstring& library )";
        HANDLE processHandle = OpenProcess( PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, (DWORD)process );
        if (processHandle != nullptr) {
            size_t fileNameSize = ((library.size() + 1) * sizeof(wchar_t));
            wchar_t* fileName = static_cast<wchar_t*>( VirtualAllocEx( processHandle, NULL, fileNameSize, MEM_COMMIT, PAGE_READWRITE ) );
            if (fileName != nullptr) {
                if (WriteProcessMemory( processHandle, fileName, library.c_str(), fileNameSize, NULL)) {
                    HMODULE module = GetModuleHandleW( L"Kernel32" );
                    if (module != nullptr) {
                        PTHREAD_START_ROUTINE function = reinterpret_cast<PTHREAD_START_ROUTINE>( GetProcAddress( module, "LoadLibraryW" ) );
                        if (function != nullptr) {
                            HANDLE threadHandle = CreateRemoteThread( processHandle, nullptr, 0, function, fileName, CREATE_SUSPENDED, nullptr );
                            // Communicate session ID and main thread ID via session ID file...
                            recordSessionInfo(sessionDirectory, session, process, thread );
                            if (threadHandle != nullptr) {
                                auto processPatched = AccessEvent( "ProcessPatched", session, process );
                                if (ResumeThread( threadHandle ) == 1) {
                                    // Wait for remote thread to indicate process has been patched...
                                    if (!EventWait( processPatched )) throw exceptionText( signature, "Failed to synchronize with remote thread" );
                                } else throw exceptionText( signature, "Failed to resume remote thread" );
                                ReleaseEvent( processPatched );
                                CloseHandle( threadHandle );
                            } else throw exceptionText( signature, "Failed to create remote thread" );
                        } else throw exceptionText( signature, "Failed to access LoadLibraryW function pointer" );
                    } else throw exceptionText( signature, "Failed to access Kernel32 module" );
                } else throw exceptionText( signature, "Failed to write to remote memory" );
            } else throw exceptionText( signature, "Failed to allocate remote memory" );
            CloseHandle( processHandle );
        } else throw string( signature ) + "Failed to open target process";
    }

} // namespace accessMonitor
