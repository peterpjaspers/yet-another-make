#include "Monitor.h"
#include "MonitorThreadsAndProcesses.h"
#include "Process.h"
#include "Session.h"
#include "MonitorLogging.h"
#include "Inject.h"
#include "FileNaming.h"
#include "Patch.h"

#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <fstream>
#include "../detours/inc/detours.h"

// ToDo: Use GetBinaryType to determine whether to inject 32-bit or 64-bit DLL
// ToDo: Single definition/declaration of accessMonitor file name/path...
// ToDo: Implenent for Linux and MacOS, current implementation is Windows only...

using namespace std;
using namespace filesystem;

namespace AccessMonitor {

    namespace {

        class ProcessGuard : public MonitorGuard {
        public:
            ProcessGuard() : MonitorGuard( Session::processAccess() ) {}
        };

        const std::string patchDLLFile( "C:/Users/philv/Code/yam/yet-another-make/accessMonitor/dll/accessMonitor64" );

        string exceptionText( const string& signature, const string& message ) {
            stringstream ss;
            ss << signature << " - " << message << "! [ " << GetLastError() << " ]";
            return ss.str();
        }

        void injectProcess( LPPROCESS_INFORMATION processInfo ) {
            const char* signature( "void injectProcess( LPPROCESS_INFORMATION processInfo )" );
            ProcessID process = GetProcessID( processInfo->dwProcessId );
            if (debugLog( General )) debugRecord() << L"MonitorThreadsAndProcesses - Injecting DLL " << wstring( patchDLLFile.begin(), patchDLLFile.end() ) << " in process with ID " << process << "..." << record;
            inject( patchDLLFile, process, Session::current() );
        }

        inline wostream& debugMessage( const char* function ) {
            return debugRecord() << L"MonitorThreadsAndProcesses - " << function << "( ";
        }
        inline wostream& debugMessage( const char* function, bool success ) {
            DWORD errorCode = GetLastError();
            if (!success) debugRecord() << L"MonitorThreadsAndProcesses - " << function << L" failed with  error : " << errorString( errorCode ) << record;
            return debugMessage( function );
        }
        inline wostream& debugMessage( const char* function, HANDLE handle ) {
            return debugMessage( function, (handle != INVALID_HANDLE_VALUE) );
        }

        wstring appNameA( const char* name ) {
            if (name != nullptr) {
                wstringstream stream;
                stream << name << L", ";
                return stream.str();
            }
            return wstring( L"... " );
        }
        wstring appNameW( const wchar_t* name ) {
            if (name != nullptr) {
                wstringstream stream;
                stream << name << L", ";
                return stream.str();
            }
            return wstring( L"... " );
        }

        // A created thread main function is called indirectly via a wrapper function that adds and
        // subsequently removes the created thread from the session thread administration.
        struct WrapperArguments {
            LPTHREAD_START_ROUTINE  main;
            LPVOID                  parameter;
            Session*                session;
        };

        DWORD threadWrapper( void* argument ) {
            auto args = static_cast<const WrapperArguments*>( argument );
            args->session->addThread();
            auto result = args->main( args->parameter );
            args->session->removeThread();
            LocalFree( argument );
            return result;
        }

        HANDLE PatchCreateThread(
            LPSECURITY_ATTRIBUTES   lpThreadAttributes,
            SIZE_T                  dwStackSize,
            LPTHREAD_START_ROUTINE  lpStartAddress,
            LPVOID                  lpParameter,
            DWORD                   dwCreationFlags,
            LPDWORD                 lpThreadId
        ) {
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            auto args = static_cast<WrapperArguments*>( LocalAlloc( LPTR, sizeof( WrapperArguments ) ) );
            args->main = lpStartAddress;
            args->parameter = lpParameter;
            args->session = Session::current();
            auto handle = patchOriginal( PatchCreateThread )( lpThreadAttributes, dwStackSize, threadWrapper, args, (dwCreationFlags | CREATE_SUSPENDED), lpThreadId );
            ProcessGuard guard;
            if ( guard() ) {
                auto thread = GetThreadID( GetThreadID( *lpThreadId ) );
                if (debugLog( PatchExecution )) debugMessage( "CreateThread", handle ) << ", ... ) with ID " << thread << record;
            }
            if (resume) ResumeThread( handle );
            return handle;
        }
        void PatchExitThread(
            DWORD   dwExitCode
        ) {
            ProcessGuard guard;
            if ( guard() ) {
                auto thread = CurrentThreadID();
                if (debugLog( PatchExecution )) debugMessage( "ExitThread" ) << dwExitCode << " ) with ID "  << thread << record;
            }
            patchOriginal( PatchExitThread )( dwExitCode );
        }
        BOOL PatchTerminateThread(
            HANDLE  hThread,
            DWORD   dwExitCode
        ) {
            ProcessGuard guard;
            if ( guard() ) {
                auto thread = GetThreadID( GetThreadId( hThread ) );
                if (debugLog( PatchExecution )) debugMessage( "TerminateThread" ) << thread << ", "  << dwExitCode << " )" << record;
            }
            return patchOriginal( PatchTerminateThread )( hThread, dwExitCode );
        }
        BOOL PatchCreateProcessA(
            LPCSTR                lpApplicationName,
            LPSTR                 lpCommandLine,
            LPSECURITY_ATTRIBUTES lpProcessAttributes,
            LPSECURITY_ATTRIBUTES lpThreadAttributes,
            BOOL                  bInheritHandles,
            DWORD                 dwCreationFlags,
            LPVOID                lpEnvironment,
            LPCSTR                lpCurrentDirectory,
            LPSTARTUPINFOA        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            auto created = patchOriginal( PatchCreateProcessA )( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessA", created ) << appNameA( lpApplicationName ) << lpCommandLine << "  ... ) with ID " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation );
            }
            if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        BOOL PatchCreateProcessW(
            LPCWSTR               lpApplicationName,
            LPWSTR                lpCommandLine,
            LPSECURITY_ATTRIBUTES lpProcessAttributes,
            LPSECURITY_ATTRIBUTES lpThreadAttributes,
            BOOL                  bInheritHandles,
            DWORD                 dwCreationFlags,
            LPVOID                lpEnvironment,
            LPCWSTR               lpCurrentDirectory,
            LPSTARTUPINFOW        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            // auto created = DetourCreateProcessWithDllExW(
            //     lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles,
            //     dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation,
            //     patchDLLFile.c_str(), patchOriginal( PatchCreateProcessW )
            // );
            auto created = patchOriginal( PatchCreateProcessW )( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessW", created ) << appNameW( lpApplicationName ) << lpCommandLine << ", ... ) with ID " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation );
            }
            if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        BOOL PatchCreateProcessAsUserA(
            HANDLE                hToken,
            LPCSTR                lpApplicationName,
            LPSTR                 lpCommandLine,
            LPSECURITY_ATTRIBUTES lpProcessAttributes,
            LPSECURITY_ATTRIBUTES lpThreadAttributes,
            BOOL                  bInheritHandles,
            DWORD                 dwCreationFlags,
            LPVOID                lpEnvironment,
            LPCSTR                lpCurrentDirectory,
            LPSTARTUPINFOA        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            auto created = patchOriginal( PatchCreateProcessAsUserA )( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessAsUserA", created ) << appNameA( lpApplicationName ) << lpCommandLine << ", ... ) with ID " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation );
            }
            if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        BOOL PatchCreateProcessAsUserW(
            HANDLE                hToken,
            LPCWSTR               lpApplicationName,
            LPWSTR                lpCommandLine,
            LPSECURITY_ATTRIBUTES lpProcessAttributes,
            LPSECURITY_ATTRIBUTES lpThreadAttributes,
            BOOL                  bInheritHandles,
            DWORD                 dwCreationFlags,
            LPVOID                lpEnvironment,
            LPCSTR                lpCurrentDirectory,
            LPSTARTUPINFOA        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            auto created = patchOriginal( PatchCreateProcessAsUserW )( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessAsUserW", created ) << appNameW( lpApplicationName )  << lpCommandLine << ", ... ) with ID " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation );
            }
            if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        BOOL PatchCreateProcessWithLogonW(
            LPCWSTR               lpUsername,
            LPCWSTR               lpDomain,
            LPCWSTR               lpPassword,
            DWORD                 dwLogonFlags,
            LPCWSTR               lpApplicationName,
            LPWSTR                lpCommandLine,
            DWORD                 dwCreationFlags,
            LPVOID                lpEnvironment,
            LPCSTR                lpCurrentDirectory,
            LPSTARTUPINFOA        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            auto created = patchOriginal( PatchCreateProcessWithLogonW )( lpUsername, lpDomain, lpPassword, dwLogonFlags, lpApplicationName, lpCommandLine, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessWithLogonW", created ) << appNameW( lpApplicationName )  << lpCommandLine << ", ... ) with ID " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation );
            }
            if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        BOOL PatchCreateProcessWithTokenW(
            HANDLE                hToken,
            DWORD                 swLogonFlags,
            LPCWSTR               lpApplicationName,
            LPWSTR                lpCommandLine,
            DWORD                 dwCreationFlags,
            LPVOID                lpEnvironment,
            LPCSTR                lpCurrentDirectory,
            LPSTARTUPINFOA        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            auto created = patchOriginal( PatchCreateProcessWithTokenW )( hToken, swLogonFlags, lpApplicationName, lpCommandLine, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessWithTokenW", created ) << appNameW( lpApplicationName )  << lpCommandLine << ", ... )" << record;
                injectProcess( lpProcessInformation );
            }
            if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
            return created;
        }

        void PatchExitProcess( unsigned int exitCode ) {
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) {
                    char fileName[ MaxFileName ] = "";
                    int size = 0;
                    HANDLE handle = OpenProcess( READ_CONTROL, false, GetCurrentProcessId() );
                    if (handle != NULL) {
                        // ToDo: Understand how to retrieve process executable name 
                        size = GetModuleFileNameExA( handle, NULL, fileName, MaxFileName );
                        CloseHandle( handle );
                        if (debugLog( PatchExecution )) {
                            if (size == 0) {
                                debugMessage( "ExitProcess" ) << exitCode << L" ) Executable path could not be determined [ " << GetLastError() << L" ]" << record;
                            } else {
                                debugMessage( "ExitProcess" ) << exitCode << L" ) Executable " << fileName << record;
                            }
                        }
                    } else if (debugLog( PatchExecution )) {
                        debugMessage( "ExitProcess" ) << exitCode  << L" ) Failed to access process [ " << GetLastError() << L" ]" << record;
                    }
                }
            }
            patchOriginal( PatchExitProcess )( exitCode );
        }
        HMODULE PatchLoadLibraryA(
            LPCSTR lpLibFileName
        ) {
            auto library = patchOriginal( PatchLoadLibraryA )( lpLibFileName );
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "LoadLibraryA" ) << lpLibFileName << L" )" << record;
            }
            return library;
        }
        HMODULE PatchLoadLibraryW(
            LPCWSTR lpLibFileName
        ) {
            auto library = patchOriginal( PatchLoadLibraryW )( lpLibFileName );
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "LoadLibraryW" ) << lpLibFileName << L" )" << record;
            }
            return library;
        }
        HMODULE PatchLoadLibraryExA(
            LPCSTR lpLibFileName,
            HANDLE hFile,
            DWORD  dwFlags
        ) {
            auto library = patchOriginal( PatchLoadLibraryExA )( lpLibFileName, hFile, dwFlags );
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "LoadLibraryExA" ) << lpLibFileName  << ", ... )" << record;
            }
            return library;
        }
        HMODULE PatchLoadLibraryExW(
            LPCWSTR lpLibFileName,
            HANDLE hFile,
            DWORD  dwFlags
        ) {
            auto library = patchOriginal( PatchLoadLibraryExW )( lpLibFileName, hFile, dwFlags );
            ProcessGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "LoadLibraryExW" ) << lpLibFileName << L", ... )" << record;
            }
            return library;
        }

        struct Registration {
            const char* library;
            const char* name;
            PatchFunction patch;
        };

        const vector<Registration> registrations = {
            { "kernel32", "CreateThread", (PatchFunction)PatchCreateThread },
            { "kernel32", "ExitThread", (PatchFunction)PatchExitThread },
            { "kernel32", "TerminateThread", (PatchFunction)PatchTerminateThread },
            { "kernel32", "CreateProcessA", (PatchFunction)PatchCreateProcessA },
            { "kernel32", "CreateProcessW", (PatchFunction)PatchCreateProcessW },
            { "kernel32", "CreateProcessAsUserA", (PatchFunction)PatchCreateProcessAsUserA },
            { "kernel32", "CreateProcessAsUserW", (PatchFunction)PatchCreateProcessAsUserW },
            { "Advapi32", "CreateProcessWithLogonW", (PatchFunction)PatchCreateProcessWithLogonW },
            { "Advapi32", "CreateProcessWithTokenW", (PatchFunction)PatchCreateProcessWithTokenW },
            { "kernel32", "ExitProcess", (PatchFunction)PatchExitProcess },
            { "kernel32", "LoadLibraryA", (PatchFunction)PatchLoadLibraryA },
            { "kernel32", "LoadLibraryW", (PatchFunction)PatchLoadLibraryW },
            { "kernel32", "LoadLibraryExA", (PatchFunction)PatchLoadLibraryExA },
            { "kernel32", "LoadLibraryExW", (PatchFunction)PatchLoadLibraryExW },
        };

    } // namespace (anonymous)

    void registerProcessesAndThreads() {
        for ( const auto reg : registrations ) registerPatch( reg.library, reg.name, reg.patch );
    }
    void unregisterProcessesAndThreads() {
        for ( const auto reg : registrations ) unregisterPatch( reg.name );
    }

} // namespace AccessMonitor