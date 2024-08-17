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

        bool enterMonitor() {
            auto guard = SessionThreadAndProcessGuard();
            if ((guard != nullptr) && !guard->monitoring) {
                guard->monitoring = true;
                guard->errorCode = GetLastError();
                return true;
            }
            return false;
        }
        void exitMonitor() {
            auto guard = SessionThreadAndProcessGuard();
            guard->monitoring = false;
            SetLastError( guard->errorCode );
        }

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

        struct ThreadArguments {
            LPTHREAD_START_ROUTINE  function;
            LPVOID                  parameter;
            SessionID               session;
            std::filesystem::path   sessionDirectory;
            LogFile*                eventLog;
            LogFile*                debugLog;
        };

        DWORD threadPatch( void* argument ) {
            auto args = static_cast<const ThreadArguments*>( argument );
            AddThreadToSession( args->session, args->sessionDirectory, args->eventLog, args->debugLog );
            auto result = args->function( args-> parameter );
            RemoveThreadFromSession();
            LocalFree( argument );
            return result;
        }
        typedef HANDLE(*TypeCreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
        HANDLE PatchCreateThread(
            LPSECURITY_ATTRIBUTES   lpThreadAttributes,
            SIZE_T                  dwStackSize,
            LPTHREAD_START_ROUTINE  lpStartAddress,
            LPVOID                  lpParameter,
            DWORD                   dwCreationFlags,
            LPDWORD                 lpThreadId
        ) {
            TypeCreateThread function = reinterpret_cast<TypeCreateThread>(original( (PatchFunction)PatchCreateThread ));
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            auto args = static_cast<ThreadArguments*>( LocalAlloc( LPTR, sizeof( ThreadArguments ) ) );
            args->function = lpStartAddress;
            args->parameter = lpParameter;
            args->session = CurrentSessionID();
            args->sessionDirectory = CurrentSessionDirectory();
            args->eventLog = SessionEventLog();
            args->debugLog = SessionDebugLog();
            HANDLE handle = function( lpThreadAttributes, dwStackSize, threadPatch, args, (dwCreationFlags | CREATE_SUSPENDED), lpThreadId );
            if (enterMonitor()) {
                auto thread = GetThreadID( GetThreadID( *lpThreadId ) );
                if (debugLog( PatchExecution )) debugMessage( "CreateThread", handle ) << ", ... ) -> " << thread << record;
                exitMonitor();
            }
            if (resume) ResumeThread( handle );
            return handle;
        }
        typedef void(*TypeExitThread)(DWORD);
        void PatchExitThread(
            DWORD   dwExitCode
        ) {
            TypeExitThread function = reinterpret_cast<TypeExitThread>(original( (PatchFunction)PatchExitThread ));
            if (enterMonitor()) {
                auto thread = CurrentThreadID();
                if (debugLog( PatchExecution )) debugMessage( "ExitThread" ) << dwExitCode << " ) on "  << thread << record;
                exitMonitor();
            }
            function( dwExitCode );
        }
        typedef BOOL(*TypeTerminateThread)(HANDLE,DWORD);
        BOOL PatchTerminateThread(
            HANDLE  hThread,
            DWORD   dwExitCode
        ) {
            TypeTerminateThread function = reinterpret_cast<TypeTerminateThread>(original( (PatchFunction)PatchTerminateThread ));
            if (enterMonitor()) {
                auto thread = GetThreadID( GetThreadId( hThread ) );
                if (debugLog( PatchExecution )) debugMessage( "TerminateThread" ) << thread << ", "  << dwExitCode << " )" << record;
                exitMonitor();
            }
            return function( hThread, dwExitCode );
        }
        typedef BOOL(*TypeCreateProcessA)(LPCSTR,LPSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
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
            TypeCreateProcessA function = reinterpret_cast<TypeCreateProcessA>(original( (PatchFunction)PatchCreateProcessA ));
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            BOOL created = function( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (enterMonitor()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessA", created ) << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
                exitMonitor();
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        typedef BOOL(*TypeCreateProcessW)(LPCWSTR,LPWSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
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
            TypeCreateProcessW function = reinterpret_cast<TypeCreateProcessW>(original( (PatchFunction)PatchCreateProcessW ));
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            BOOL created = function( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (enterMonitor()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessW", created ) << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
                exitMonitor();
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        typedef BOOL(*TypeCreateProcessAsUserA)(HANDLE,LPCSTR,LPSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
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
            TypeCreateProcessAsUserA function = reinterpret_cast<TypeCreateProcessAsUserA>(original( (PatchFunction)PatchCreateProcessAsUserA));
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            BOOL created = function( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (enterMonitor()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessAsUserA", created ) << " ..., " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
                exitMonitor();
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        typedef BOOL(*TypeCreateProcessAsUserW)(HANDLE,LPCWSTR,LPWSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
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
            TypeCreateProcessAsUserW function = reinterpret_cast<TypeCreateProcessAsUserW>(original( (PatchFunction)PatchCreateProcessAsUserW));
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            BOOL created = function( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (enterMonitor()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessAsUserW", created ) << " ..., " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
                exitMonitor();
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        typedef BOOL(*TypeCreateProcessWithLogonW)(LPCWSTR,LPCWSTR,LPCWSTR,DWORD,LPCWSTR,LPWSTR,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
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
            TypeCreateProcessWithLogonW function = reinterpret_cast<TypeCreateProcessWithLogonW>(original( (PatchFunction)PatchCreateProcessAsUserW));
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            BOOL created = function( lpUsername, lpDomain, lpPassword, dwLogonFlags, lpApplicationName, lpCommandLine, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (enterMonitor()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessWithLogonW", created ) << " ..., " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << lpProcessInformation->dwProcessId << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
                exitMonitor();
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        typedef  BOOL(*TypeCreateProcessWithTokenW)(HANDLE,DWORD,LPCWSTR,LPWSTR,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
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
            TypeCreateProcessWithTokenW function = reinterpret_cast<TypeCreateProcessWithTokenW>(original( (PatchFunction)PatchCreateProcessAsUserW));
            bool resume = !(dwCreationFlags & CREATE_SUSPENDED);
            BOOL created = function( hToken, swLogonFlags, lpApplicationName, lpCommandLine, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (enterMonitor()) {
                if (debugLog( PatchExecution )) debugMessage( "CreateProcessWithTokenW", created ) << " ..., " << lpApplicationName << ", " << lpCommandLine << ", ... )" << record;
                injectProcess( lpProcessInformation->dwProcessId, lpProcessInformation->dwThreadId );
                exitMonitor();
            }
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return created;
        }
        typedef void(*TypeExitProcess)(unsigned int);
        void PatchExitProcess( unsigned int exitCode ) {
            TypeExitProcess function = reinterpret_cast<TypeExitProcess>(original( (PatchFunction)PatchExitProcess));
            if (enterMonitor()) {
                char fileName[ MaxFileName ];
                HANDLE handle = OpenProcess( READ_CONTROL, false, GetCurrentProcessId() );
                if (handle != NULL) {
                    GetModuleFileNameExA( handle, NULL, fileName, MaxFileName );
                    CloseHandle( handle );
                } else strcpy( fileName, "Executable path could not be determined." );
                if (debugLog( PatchExecution )) debugMessage( "ExitProcess" ) << exitCode << " ) - " << fileName << record;
                exitMonitor();
            }
            function( exitCode );
        }
        typedef BOOL(*TypeTerminateProcess)(HANDLE,DWORD);
        BOOL PatchTerminateProcess( HANDLE handle, DWORD exitCode ) {
            TypeTerminateProcess function = reinterpret_cast<TypeTerminateProcess>(original( (PatchFunction)PatchTerminateProcess));
            if (enterMonitor()) {
                char fileName[ MaxFileName ];
                GetModuleFileNameExA( handle, NULL, fileName, MaxFileName );
                if (debugLog( PatchExecution )) debugMessage( "TerminateProcess" ) << fileName << ", " << exitCode << " )" << record;
                exitMonitor();
            }
            return function( handle, exitCode );
        }
        typedef HMODULE(*TypeLoadLibraryA)(LPCSTR);
        HMODULE PatchLoadLibraryA(
            LPCSTR lpLibFileName
        ) {
            auto function = reinterpret_cast<TypeLoadLibraryA>(original( (PatchFunction)PatchLoadLibraryA));
            ThreadID thread = GetThreadID( GetCurrentThreadId() );
            HMODULE library = function( lpLibFileName );
            if (enterMonitor()) {
                ThreadID thread = GetThreadID( GetCurrentThreadId() );
                exitMonitor();
            }
            return library;
        }
        typedef HMODULE(*TypeLoadLibraryW)(LPCWSTR);
        HMODULE PatchLoadLibraryW(
            LPCWSTR lpLibFileName
        ) {
            auto function = reinterpret_cast<TypeLoadLibraryW>(original( (PatchFunction)PatchLoadLibraryW));
            HMODULE library = function( lpLibFileName );
            if (enterMonitor()) {
                ThreadID thread = GetThreadID( GetCurrentThreadId() );
                exitMonitor();
            }
            return library;
        }
        typedef HMODULE(*TypeLoadLibraryExA)(LPCSTR,HANDLE,DWORD);
        HMODULE PatchLoadLibraryExA(
            LPCSTR lpLibFileName,
            HANDLE hFile,
            DWORD  dwFlags
        ) {
            auto function = reinterpret_cast<TypeLoadLibraryExA>(original( (PatchFunction)PatchLoadLibraryExA));
            HMODULE library = function( lpLibFileName, hFile, dwFlags );
            if (enterMonitor()) {
                ThreadID thread = GetThreadID( GetCurrentThreadId() );
                exitMonitor();
            }
            return library;
        }
        typedef HMODULE(*TypeLoadLibraryExW)(LPCWSTR,HANDLE,DWORD);
        HMODULE PatchLoadLibraryExW(
            LPCWSTR lpLibFileName,
            HANDLE hFile,
            DWORD  dwFlags
        ) {
            auto function = reinterpret_cast<TypeLoadLibraryExW>(original( (PatchFunction)PatchLoadLibraryExW));
            HMODULE library = function( lpLibFileName, hFile, dwFlags );
            if (enterMonitor()) {
                ThreadID thread = GetThreadID( GetCurrentThreadId() );
                exitMonitor();
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