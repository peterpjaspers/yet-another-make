#include "Inject.h"

#include <windows.h>

using namespace std;

namespace AccessMonitor {
    
    // Injects a DLL library in a (remote) process via a remote thread in the target process
    // that excutes LoadLibrary as its (main) function. The DLLMain entry point of the library is
    // called as a result of loading the DLL.
    // Throws one of a number of failure specific exceptions.
    void inject( PID pid, const wstring& library ) {
        const char* signature = "void inject( PID pid, const wstring& library )";
        HANDLE process = OpenProcess( PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid );
        if (process != nullptr) {
            size_t fileNameSize = (library.size() + sizeof(wchar_t));
            wchar_t* fileName = static_cast<wchar_t*>( VirtualAllocEx( process, NULL, fileNameSize, MEM_COMMIT, PAGE_READWRITE ) );
            if (fileName != nullptr) {
                if (WriteProcessMemory( process, fileName, library.c_str(), fileNameSize, NULL)) {
                    HMODULE module = GetModuleHandleW( L"Kernel32" );
                    if (module != nullptr) {
                        PTHREAD_START_ROUTINE function = PTHREAD_START_ROUTINE( GetProcAddress( module, "LoadLibraryW" ) );
                        if (function != nullptr) {
                            HANDLE thread = CreateRemoteThread( process, NULL, 0, function, fileName, 0, NULL);
                            if (thread != nullptr) {
                                CloseHandle( thread );
                            } else throw string( signature ) + " - Failed to create remote thread!";
                        } else throw string( signature ) + " - Failed to access LoadLibraryW function pointer!";
                    } else throw string( signature ) + " - Failed to access Kernel32 module!";
                } else throw string( signature ) + " - Failed to write to remote memory!";
            } else throw string( signature ) + " - Failed to allocate remote memory!";
            CloseHandle( process );
        } else throw string( signature ) + " - Failed to open target process!";
    }

} // namespace accessMonitor
