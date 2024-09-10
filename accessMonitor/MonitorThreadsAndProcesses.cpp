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

// ToDo: Add function calling convention to all externals
// ToDo: Use GetBinaryType to determine whether to inject 32-bit or 64-bit DLL
// ToDo: Single definition/declaration of accessMonitor.dll file name/path...
// ToDo: Implenent for Linux and MacOS, current implementation is Windows only...
// ToDo: Add logging to LoadLibrary calls

using namespace std;

namespace AccessMonitor {

    namespace {

        class ProcessMonitorGuard : public MonitorGuard {
        public:
            ProcessMonitorGuard() : MonitorGuard( SessionThreadAndProcessAccess() ) {}
        };

        const std::wstring patchDLLFile( L"C:/Users/philv/Code/yam/yet-another-make/accessMonitor/dll/accessMonitor.dll" );
        // const std::wstring patchDLLFile( L"D:/Peter/github/yam/x64/Debug/dll/accessMonitorDLL.dll" );

        void injectProcess( DWORD pid, DWORD tid ) {
            ProcessID process = GetProcessID( pid );
            ThreadID main = GetThreadID( tid );
            debugRecord() << L"MonitorThreadsAndProcesses - Injecting DLL " << patchDLLFile << " in process " << process << " with main thread " << main << "..." << record;
            inject( CurrentSessionDirectory(), CurrentSessionID(), process, main, patchDLLFile );
        }

        inline wostream& debugMessage( const char* function ) {
            return debugRecord() << L"MonitorThreadsAndProcesses - " << function << "( ";
        }
        inline wostream& debugMessage( const char* function, bool success ) {
            DWORD errorCode = GetLastError();
            if (!success) debugRecord() << L"MonitorThreadsAndProcesses - " << function << L" failed with  error : " << lasErrorString( errorCode ) << record;
            return debugMessage( function );
        }
        inline wostream& debugMessage( const char* function, HANDLE handle ) {
            return debugMessage( function, (handle != INVALID_HANDLE_VALUE) );
        }

        // A created thread main function is called indirectly via a wrapper function that adds and
        // subsequently removes the created thread from the session thread administration.
        struct WrapperArguments {
            LPTHREAD_START_ROUTINE  main;
            LPVOID                  parameter;
            SessionID               session;
            std::filesystem::path   sessionDirectory;
            LogFile*                eventLog;
            LogFile*                debugLog;
        };

        DWORD threadWrapper( void* argument ) {
            auto args = static_cast<const WrapperArguments*>( argument );
            AddThreadToSession( args->session, args->sessionDirectory, args->eventLog, args->debugLog );
            auto result = args->main( args->parameter );
            RemoveThreadFromSession();
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
            args->session = CurrentSessionID();
            args->sessionDirectory = CurrentSessionDirectory();
            args->eventLog = SessionEventLog();
            args->debugLog = SessionDebugLog();
            auto handle = patchOriginal( PatchCreateThread )( lpThreadAttributes, dwStackSize, threadWrapper, args, (dwCreationFlags | CREATE_SUSPENDED), lpThreadId );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                auto thread = GetThreadID( GetThreadID( *lpThreadId ) );
                if (debugLog( PatchExecution )) debugMessage( "CreateThread", handle ) << ", ... ) -> " << thread << record;
            }
            if (resume) ResumeThread( handle );
            return handle;
        }
        void PatchExitThread(
            DWORD   dwExitCode
        ) {
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                auto thread = CurrentThreadID();
                if (debugLog( PatchExecution )) debugMessage( "ExitThread" ) << dwExitCode << " ) on "  << thread << record;
            }
            patchOriginal( PatchExitThread )( dwExitCode );
        }
        BOOL PatchTerminateThread(
            HANDLE  hThread,
            DWORD   dwExitCode
        ) {
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
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
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            auto created = patchOriginal( PatchCreateProcessA )( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessA", created ) << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
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
            LPCSTR                lpCurrentDirectory,
            LPSTARTUPINFOA        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            auto created = patchOriginal( PatchCreateProcessW )( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessW", created ) << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
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
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            auto created = patchOriginal( PatchCreateProcessAsUserA )( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessAsUserA", created ) << " ..., " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
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
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            auto created = patchOriginal( PatchCreateProcessAsUserW )( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessAsUserW", created ) << " ..., " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
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
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            auto created = patchOriginal( PatchCreateProcessWithLogonW )( lpUsername, lpDomain, lpPassword, dwLogonFlags, lpApplicationName, lpCommandLine, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessWithLogonW", created ) << " ..., " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
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
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            auto created = patchOriginal( PatchCreateProcessWithTokenW )( hToken, swLogonFlags, lpApplicationName, lpCommandLine, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessWithTokenW", created ) << " ..., " << lpApplicationName << ", " << lpCommandLine << ", ... )" << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        void PatchExitProcess( unsigned int exitCode ) {
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                char fileName[ MaxFileName ];
                HANDLE handle = OpenProcess( READ_CONTROL, false, GetCurrentProcessId() );
                if (handle != NULL) {
                    GetModuleFileNameExA( handle, NULL, fileName, MaxFileName );
                    CloseHandle( handle );
                } else strcpy( fileName, "Executable path could not be determined." );
                if (debugLog( PatchExecution )) debugMessage( "ExitProcess" ) << exitCode << " ) - " << fileName << record;
            }
            patchOriginal( PatchExitProcess )( exitCode );
        }
        BOOL PatchTerminateProcess( HANDLE handle, DWORD exitCode ) {
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                char fileName[ MaxFileName ];
                GetModuleFileNameExA( handle, NULL, fileName, MaxFileName );
                if (debugLog( PatchExecution )) debugMessage( "TerminateProcess" ) << fileName << ", " << exitCode << " )" << record;
            }
            return patchOriginal( PatchTerminateProcess )( handle, exitCode );
        }
        HMODULE PatchLoadLibraryA(
            LPCSTR lpLibFileName
        ) {
            auto library = patchOriginal( PatchLoadLibraryA )( lpLibFileName );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                ThreadID thread = GetThreadID( GetCurrentThreadId() );
            }
            return library;
        }
        HMODULE PatchLoadLibraryW(
            LPCWSTR lpLibFileName
        ) {
            auto library = patchOriginal( PatchLoadLibraryW )( lpLibFileName );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                ThreadID thread = GetThreadID( GetCurrentThreadId() );
            }
            return library;
        }
        HMODULE PatchLoadLibraryExA(
            LPCSTR lpLibFileName,
            HANDLE hFile,
            DWORD  dwFlags
        ) {
            auto library = patchOriginal( PatchLoadLibraryExA )( lpLibFileName, hFile, dwFlags );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                ThreadID thread = GetThreadID( GetCurrentThreadId() );
            }
            return library;
        }
        HMODULE PatchLoadLibraryExW(
            LPCWSTR lpLibFileName,
            HANDLE hFile,
            DWORD  dwFlags
        ) {
            auto library = patchOriginal( PatchLoadLibraryExW )( lpLibFileName, hFile, dwFlags );
            ProcessMonitorGuard guard;
            if (guard.monitoring()) {
                ThreadID thread = GetThreadID( GetCurrentThreadId() );
            }
            return library;
        }

   } // namespace (anonymous)

    void registerProcessesAndThreads() {
        registerPatch( "CreateThread", (PatchFunction)PatchCreateThread );
        registerPatch( "ExitThread", (PatchFunction)PatchExitThread );
        registerPatch( "TerminateThread", (PatchFunction)PatchTerminateThread );
        registerPatch( "CreateProcessA", (PatchFunction)PatchCreateProcessA );
        registerPatch( "CreateProcessW", (PatchFunction)PatchCreateProcessW );
        registerPatch( "CreateProcessAsUserA", (PatchFunction)PatchCreateProcessAsUserA );
        registerPatch( "CreateProcessAsUserW", (PatchFunction)PatchCreateProcessAsUserW );
        registerPatch( "CreateProcessWithLogonW", (PatchFunction)PatchCreateProcessWithLogonW );
        registerPatch( "CreateProcessWithTokenW", (PatchFunction)PatchCreateProcessWithTokenW );
        registerPatch( "ExitProcess", (PatchFunction)PatchExitProcess );
        registerPatch( "TerminateProcess", (PatchFunction)PatchTerminateProcess );
        registerPatch( "LoadLibraryA", (PatchFunction)PatchLoadLibraryA );
        registerPatch( "LoadLibraryW", (PatchFunction)PatchLoadLibraryW );
        registerPatch( "LoadLibraryExA", (PatchFunction)PatchLoadLibraryExA );
        registerPatch( "LoadLibraryExW", (PatchFunction)PatchLoadLibraryExW );
    }
    void unregisterProcessCreation() {
        unregisterPatch( "CreateThread" );
        unregisterPatch( "ExitThread" );
        unregisterPatch( "TerminateThread" );
        unregisterPatch( "CreateProcessA" );
        unregisterPatch( "CreateProcessW" );
        unregisterPatch( "CreateProcessAsUserA" );
        unregisterPatch( "CreateProcessAsUserW" );
        unregisterPatch( "CreateProcessWithLogonW" );
        unregisterPatch( "CreateProcessWithTokenW" );
        unregisterPatch( "ExitProcess" );
        unregisterPatch( "TerminateProcess" );
        unregisterPatch( "LoadLibraryA" );
        unregisterPatch( "LoadLibraryW" );
        unregisterPatch( "LoadLibraryExA" );
        unregisterPatch( "LoadLibraryExW" );
    }  
} // namespace AccessMonitor