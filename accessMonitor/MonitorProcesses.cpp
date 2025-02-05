#include "Monitor.h"
#include "MonitorProcesses.h"
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
//
using namespace std;
using namespace filesystem;

namespace AccessMonitor {

    namespace {
        const unsigned long IndexBase = 80;
        enum{
            IndexCreateThread               = IndexBase + 0,
            IndexExitThread                 = IndexBase + 1,
            IndexTerminateThread            = IndexBase + 2,
            IndexCreateProcessA             = IndexBase + 3,
            IndexCreateProcessW             = IndexBase + 4,
            IndexCreateProcessAsUserA       = IndexBase + 5,
            IndexCreateProcessAsUserW       = IndexBase + 6,
            IndexCreateProcessWithLogonW    = IndexBase + 7,
            IndexCreateProcessWithTokenW    = IndexBase + 8,
            IndexExitProcess                = IndexBase + 9,
            IndexLoadLibraryA               = IndexBase + 10,
            IndexLoadLibraryW               = IndexBase + 11,
            IndexLoadLibraryExA             = IndexBase + 12,
            IndexLoadLibraryExW             = IndexBase + 14,
        };

        //const std::string patchDLLFile("C:/Users/philv/Code/yam/yet-another-make/accessMonitor/dll/accessMonitor64");
        const std::string patchDLLFile("D:/Peter/Github/yam/x64/Debug/accessMonitorDll.dll");

        string exceptionText( const string& signature, const string& message ) {
            stringstream ss;
            ss << signature << " - " << message << "! [ " << GetLastError() << " ]";
            return ss.str();
        }

        void injectProcess( LPPROCESS_INFORMATION processInfo ) {
            const char* signature( "void injectProcess( LPPROCESS_INFORMATION processInfo )" );
            ProcessID process = GetProcessID( processInfo->dwProcessId );
            if (debugLog( General )) debugRecord() << L"MonitorProcesses - Injecting DLL " << wstring( patchDLLFile.begin(), patchDLLFile.end() ) << " in process with ID " << process << "..." << record;
            inject( patchDLLFile, process, Session::current() );
        }

        inline wostream& debugMessage( const char* function ) {
            return debugRecord() << L"MonitorProcesses - " << function << "( ";
        }
        inline wostream& debugMessage( const char* function, bool success ) {
            DWORD errorCode = GetLastError();
            if (!success) debugRecord() << L"MonitorProcesses - " << function << L" failed with  error : " << errorString( errorCode ) << record;
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
            DWORD result = 0;
            auto args = static_cast<const WrapperArguments*>( argument );
            if (args->session != nullptr) {
                args->session->addThread();
                result = args->main(args->parameter);
                args->session->removeThread();
            }
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
            auto original( patchOriginal( PatchCreateThread, IndexCreateThread ) );
            MonitorGuard guard( Session::monitorProcessAccess( false ) );
            if ( guard() ) {
                bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
                auto args = static_cast<WrapperArguments*>( LocalAlloc( LPTR, sizeof( WrapperArguments ) ) );
                args->main = lpStartAddress;
                args->parameter = lpParameter;
                args->session = Session::current();
                auto handle = original( lpThreadAttributes, dwStackSize, threadWrapper, args, (dwCreationFlags | CREATE_SUSPENDED), lpThreadId );
                guard.error( GetLastError() );
                auto thread = GetThreadID( *lpThreadId );
                if (debugLog( PatchExecution )) debugMessage( "CreateThread", handle ) << ", ... ) with ID " << thread << record;
                if (resume) ResumeThread( handle );
                return handle;
            }
            return original( lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId );
        }
        void PatchExitThread(
            DWORD   dwExitCode
        ) {
            auto original( patchOriginal( PatchExitThread, IndexExitThread ) );
            MonitorGuard guard( Session::monitorProcessAccess( false ) );
            if ( guard() ) {
                auto thread = CurrentThreadID();
                if (debugLog( PatchExecution )) debugMessage( "ExitThread" ) << dwExitCode << " ) with ID "  << thread << record;
            }
            return original( dwExitCode );
        }
        BOOL PatchTerminateThread(
            HANDLE  hThread,
            DWORD   dwExitCode
        ) {
            auto original( patchOriginal( PatchTerminateThread, IndexTerminateThread ) );
            MonitorGuard guard( Session::monitorProcessAccess( false ) );
            if ( guard() ) {
                auto thread = GetThreadID( GetThreadId( hThread ) );
                if (debugLog( PatchExecution )) debugMessage( "TerminateThread" ) << thread << ", "  << dwExitCode << " )" << record;
            }
            return original( hThread, dwExitCode );
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
            auto original( patchOriginal( PatchCreateProcessA, IndexCreateProcessA ) );
            MonitorGuard guard( Session::monitorProcessAccess( false ) );
            if ( guard() ) {
                auto created = original( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
                guard.error( GetLastError() );
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessA", created ) << appNameA( lpApplicationName ) << lpCommandLine << "  ... ) with ID " << lpProcessInformation->dwProcessId << record;
                if (created) {
                    injectProcess( lpProcessInformation );
                    if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
                }
                return created;
            }
            return original( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
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
            auto original( patchOriginal( PatchCreateProcessW, IndexCreateProcessW ) );
            MonitorGuard guard( Session::monitorProcessAccess( false ) );
            if ( guard() ) {
                auto created = original( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
                guard.error( GetLastError() );
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessW", created ) << appNameW( lpApplicationName ) << lpCommandLine << ", ... ) with ID " << lpProcessInformation->dwProcessId << record;
                if (created) {
                    injectProcess( lpProcessInformation );
                    if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
                }
                return created;
            }
            return original( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
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
            auto original( patchOriginal( PatchCreateProcessAsUserA, IndexCreateProcessAsUserA ) );
            MonitorGuard guard( Session::monitorProcessAccess( false ) );
            if ( guard() ) {
                auto created = original( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
                guard.error( GetLastError() );
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessAsUserA", created ) << appNameA( lpApplicationName ) << lpCommandLine << ", ... ) with ID " << lpProcessInformation->dwProcessId << record;
                if (created) {
                    injectProcess( lpProcessInformation );
                    if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
                }
                return created;
            }
            return original( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
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
            LPCWSTR               lpCurrentDirectory,
            LPSTARTUPINFOW        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            auto original( patchOriginal( PatchCreateProcessAsUserW, IndexCreateProcessAsUserW ) );
            MonitorGuard guard( Session::monitorProcessAccess( false ) );
            if ( guard() ) {
                auto created = original( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
                guard.error( GetLastError() );
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessAsUserW", created ) << appNameW( lpApplicationName )  << lpCommandLine << ", ... ) with ID " << lpProcessInformation->dwProcessId << record;
                if (created) {
                    injectProcess( lpProcessInformation );
                    if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
                }
                return created;
            }
            return original( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
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
            LPCWSTR               lpCurrentDirectory,
            LPSTARTUPINFOW        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            auto original( patchOriginal( PatchCreateProcessWithLogonW, IndexCreateProcessWithLogonW ) );
            MonitorGuard guard( Session::monitorProcessAccess( false ) );
            if ( guard() ) {
                auto created = original( lpUsername, lpDomain, lpPassword, dwLogonFlags, lpApplicationName, lpCommandLine, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
                guard.error( GetLastError() );
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessWithLogonW", created ) << appNameW( lpApplicationName )  << lpCommandLine << ", ... ) with ID " << lpProcessInformation->dwProcessId << record;
                if (created) {
                    injectProcess( lpProcessInformation );
                    if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
                }
                return created;
            }
            return original( lpUsername, lpDomain, lpPassword, dwLogonFlags, lpApplicationName, lpCommandLine, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
        }
        BOOL PatchCreateProcessWithTokenW(
            HANDLE                hToken,
            DWORD                 swLogonFlags,
            LPCWSTR               lpApplicationName,
            LPWSTR                lpCommandLine,
            DWORD                 dwCreationFlags,
            LPVOID                lpEnvironment,
            LPCWSTR               lpCurrentDirectory,
            LPSTARTUPINFOW        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            auto original( patchOriginal( PatchCreateProcessWithTokenW, IndexCreateProcessWithTokenW ) );
            MonitorGuard guard( Session::monitorProcessAccess( false ) );
            if ( guard() ) {
                auto created = original( hToken, swLogonFlags, lpApplicationName, lpCommandLine, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
                guard.error( GetLastError() );
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessWithTokenW", created ) << appNameW( lpApplicationName )  << lpCommandLine << ", ... )" << record;
                if (created) {
                    injectProcess( lpProcessInformation );
                    if (!(dwCreationFlags & CREATE_SUSPENDED)) ResumeThread( lpProcessInformation->hThread );
                }
                return created;
            }
            return original( hToken, swLogonFlags, lpApplicationName, lpCommandLine, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
        }

        void PatchExitProcess( unsigned int exitCode ) {
            auto original( patchOriginal( PatchExitProcess, IndexExitProcess) );
            MonitorGuard guard( Session::monitorProcessAccess( false ) );
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
            return original( exitCode );
        }
        HMODULE PatchLoadLibraryA(
            LPCSTR lpLibFileName
        ) {
            auto library = patchOriginal( PatchLoadLibraryA, IndexLoadLibraryA )( lpLibFileName );
            MonitorGuard guard( Session::monitorProcessAccess() );
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "LoadLibraryA" ) << lpLibFileName << L" )" << record;
            }
            return library;
        }
        HMODULE PatchLoadLibraryW(
            LPCWSTR lpLibFileName
        ) {
            auto library = patchOriginal( PatchLoadLibraryW, IndexLoadLibraryW )( lpLibFileName );
            MonitorGuard guard( Session::monitorProcessAccess() );
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
            auto library = patchOriginal( PatchLoadLibraryExA, IndexLoadLibraryExA )( lpLibFileName, hFile, dwFlags );
            MonitorGuard guard( Session::monitorProcessAccess() );
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
            auto library = patchOriginal( PatchLoadLibraryExW, IndexLoadLibraryExW )( lpLibFileName, hFile, dwFlags );
            MonitorGuard guard( Session::monitorProcessAccess() );
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "LoadLibraryExW" ) << lpLibFileName << L", ... )" << record;
            }
            return library;
        }

        const vector<Registration> processRegistrations = {
            { "kernel32", "CreateThread",               (PatchFunction)PatchCreateThread,               IndexCreateThread },
            { "kernel32", "ExitThread",                 (PatchFunction)PatchExitThread,                 IndexExitThread },
            { "kernel32", "TerminateThread",            (PatchFunction)PatchTerminateThread,            IndexTerminateThread },
            { "kernel32", "CreateProcessA",             (PatchFunction)PatchCreateProcessA,             IndexCreateProcessA },
            { "kernel32", "CreateProcessW",             (PatchFunction)PatchCreateProcessW,             IndexCreateProcessW },
            { "kernel32", "CreateProcessAsUserA",       (PatchFunction)PatchCreateProcessAsUserA,       IndexCreateProcessAsUserA },
            { "kernel32", "CreateProcessAsUserW",       (PatchFunction)PatchCreateProcessAsUserW,       IndexCreateProcessAsUserW },
            { "Advapi32", "CreateProcessWithLogonW",    (PatchFunction)PatchCreateProcessWithLogonW,    IndexCreateProcessWithLogonW },
            { "Advapi32", "CreateProcessWithTokenW",    (PatchFunction)PatchCreateProcessWithTokenW,    IndexCreateProcessWithTokenW },
            { "kernel32", "ExitProcess",                (PatchFunction)PatchExitProcess,                IndexExitProcess },
            { "kernel32", "LoadLibraryA",               (PatchFunction)PatchLoadLibraryA,               IndexLoadLibraryA },
            { "kernel32", "LoadLibraryW",               (PatchFunction)PatchLoadLibraryW,               IndexLoadLibraryW },
            { "kernel32", "LoadLibraryExA",             (PatchFunction)PatchLoadLibraryExA,             IndexLoadLibraryExA },
            { "kernel32", "LoadLibraryExW",             (PatchFunction)PatchLoadLibraryExW,             IndexLoadLibraryExW },
        };

    } // namespace (anonymous)

    void registerProcessAccess() {
        for ( const auto reg : processRegistrations ) registerPatch( reg.library, reg.name, reg.patch, reg.index );
    }
    void unregisterProcessAccess() {
        for ( const auto reg : processRegistrations ) unregisterPatch( reg.name );
    }

} // namespace AccessMonitor