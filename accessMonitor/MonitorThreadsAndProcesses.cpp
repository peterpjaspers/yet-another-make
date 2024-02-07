#include "MonitorThreadsAndProcesses.h"
#include "MonitorProcess.h"
#include "Patch.h"
#include "Inject.h"
#include "Log.h"

#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <fstream>

using namespace std;

namespace AccessMonitor {

    namespace {

        const wstring patchDLLFile( L"C:\\Users\\philv\\Code\\yam\\yet-another-make\\accessMonitor\\patchDLL.dll" );
        
        typedef HMODULE(*TypeLoadLibraryA)(LPCSTR);
        HMODULE PatchLoadLibraryA( LPCSTR libraryName ) {
            TypeLoadLibraryA function = reinterpret_cast<TypeLoadLibraryA>(original( (PatchFunction)PatchLoadLibraryA ));
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - LoadLibraryA( " << libraryName << " )" << record;
            auto library = function( libraryName );
            return library;
        }
        typedef HMODULE(*TypeLoadLibraryW)(LPCWSTR);
        HMODULE PatchLoadLibraryW( LPCWSTR libraryName ) {
            TypeLoadLibraryW function = reinterpret_cast<TypeLoadLibraryW>(original( (PatchFunction)PatchLoadLibraryW ));
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - LoadLibraryW( " << libraryName << " )" << record;
            auto library = function( libraryName );
            return library;
        }
        typedef HMODULE(*TypeLoadLibraryExA)(LPCSTR,HANDLE,DWORD);
        HMODULE PatchLoadLibraryExA( LPCSTR libraryName, HANDLE reserved, DWORD flags ) {
            TypeLoadLibraryExA function = reinterpret_cast<TypeLoadLibraryExA>(original( (PatchFunction)PatchLoadLibraryExA ));
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - LoadLibraryExA( " << libraryName << ", ... , " << hex << flags << " )" << dec << record;
            auto library = function( libraryName, reserved, flags );
            return library;
        }
        typedef HMODULE(*TypeLoadLibraryExW)(LPCWSTR,HANDLE,DWORD);
        HMODULE PatchLoadLibraryExW( LPCWSTR libraryName, HANDLE reserved, DWORD flags ) {
            TypeLoadLibraryExW function = reinterpret_cast<TypeLoadLibraryExW>(original( (PatchFunction)PatchLoadLibraryExW ));
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - LoadLibraryExA( " << libraryName << ", ... , " << hex << flags << " )" << dec << record;
            auto library = function( libraryName, reserved, flags );
            return library;
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
            BOOL result = function( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            ProcessID child = GetProcessID( lpProcessInformation->dwProcessId );
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - CreateProcessA( " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << record;
            inject( lpProcessInformation->dwProcessId, patchDLLFile );
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return result;
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
            BOOL result = function( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - CreateProcessW( " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << record;
            inject( lpProcessInformation->dwProcessId, patchDLLFile );
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return result;
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
            BOOL result = function( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - CreateProcessAsUserA( ... , " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << record;
            inject( lpProcessInformation->dwProcessId, patchDLLFile );
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return result;
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
            BOOL result = function( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - CreateProcessAsUserW( ... , " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << record;
            inject( lpProcessInformation->dwProcessId, patchDLLFile );
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return result;
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
            BOOL result = function( lpUsername, lpDomain, lpPassword, dwLogonFlags, lpApplicationName, lpCommandLine, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - CreateProcessWithLogonW( ... , " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << record;
            inject( lpProcessInformation->dwProcessId, patchDLLFile );
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return result;
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
            BOOL result = function( hToken, swLogonFlags, lpApplicationName, lpCommandLine, (dwCreationFlags | CREATE_SUSPENDED), lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - CreateProcessWithTokenW( ... , " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << record;
            inject( lpProcessInformation->dwProcessId, patchDLLFile );
            if (resume) ResumeThread( lpProcessInformation->hThread );
            return result;
        }
        typedef void(*TypeExitProcess)(unsigned int);
        void PatchExitProcess( unsigned int code ) {
            TypeExitProcess function = reinterpret_cast<TypeExitProcess>(original( (PatchFunction)PatchExitProcess));
            DWORD error = GetLastError();
            ProcessID pid = CurrentProcessID();
            HANDLE process = OpenProcess( READ_CONTROL, false, pid );
            if (process != NULL) {
                char fileName[ MaxFileName ];
                auto size = GetModuleFileNameExA( process, NULL, fileName, MaxFileName );
                if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - ExitProcess( " << code << " ) - " << fileName << record;
                CloseHandle( process );
            } else if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - ExitProcess( " << code << " )" << record;
            auto exit = AccessEvent( "ExitProcess", CurrentProcessID() );
            EventSignal( exit );
            EventWait( exit );
            SetLastError( error );
            function( code );
        }
        typedef BOOL(*TypeTerminateProcess)(HANDLE,DWORD);
        void PatchTerminateProcess( HANDLE process, DWORD exitCode ) {
            TypeTerminateProcess function = reinterpret_cast<TypeTerminateProcess>(original( (PatchFunction)PatchTerminateProcess));
            // ToDo: Synchronize with terminating process if it is a child of this process
            DWORD error = GetLastError();
            if (process != NULL) {
                char fileName[ MaxFileName ];
                auto size = GetModuleFileNameExA( process, NULL, fileName, MaxFileName );
                if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - TerminateProcess( " << exitCode << " ) - " << fileName << record;
            } else if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - TerminateProcess( " << exitCode << " )" << record;
            auto exit = AccessEvent( "ExitProcess", CurrentProcessID() );
            EventSignal( exit );
            EventWait( exit );
            SetLastError( error );
            BOOL result = function( process, exitCode );
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - TerminateProcess( " << process << ", " << exitCode << " )" << record;
        }

        typedef NTSTATUS(*TypeNtCreateProcess)(HANDLE*,ACCESS_MASK,OBJECT_ATTRIBUTES*,HANDLE,BOOL,HANDLE,HANDLE,HANDLE);
        NTSTATUS PatchNtCreateProcess(
            HANDLE*             ProcessHandle,
            ACCESS_MASK         DesiredAccess,
            OBJECT_ATTRIBUTES*  ObjectAttributes,
            HANDLE              ParentProcess,
            BOOL                InheritObjectTable,
            HANDLE              SectionHandle,
            HANDLE              DebugPort,
            HANDLE              TokenHandle
        ) {
            TypeNtCreateProcess function = reinterpret_cast<TypeNtCreateProcess>(original( (PatchFunction)PatchNtCreateProcess));
            NTSTATUS result = function( ProcessHandle, DesiredAccess, ObjectAttributes, ParentProcess, InheritObjectTable, SectionHandle, DebugPort, TokenHandle );
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - NtCreateProcess( ... ) -> " << result << record;
            return result;
        }
    	typedef NTSTATUS(*TypeNtCreateProcessEx)(HANDLE*,ACCESS_MASK,OBJECT_ATTRIBUTES*,HANDLE,ULONG,HANDLE,HANDLE,HANDLE,ULONG);
        NTSTATUS PatchNtCreateProcessEx(
            HANDLE*             ProcessHandle,
            ACCESS_MASK         DesiredAccess,
            OBJECT_ATTRIBUTES*  ObjectAttributes,
            HANDLE              ParentProcess,
            ULONG               Flags,
            HANDLE              SectionHandle,
            HANDLE              DebugPort,
            HANDLE              TokenHandle,
            ULONG               Reserved
        ){
            TypeNtCreateProcessEx function = reinterpret_cast<TypeNtCreateProcessEx>(original( (PatchFunction)PatchNtCreateProcessEx));
            NTSTATUS result = function( ProcessHandle, DesiredAccess, ObjectAttributes, ParentProcess, Flags, SectionHandle, DebugPort, TokenHandle, Reserved );
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - NtCreateProcessEx( ... ) -> " << result << record;
            return result;
        }
        typedef NTSTATUS(*TypeNtCreateUserProcess)(HANDLE*,HANDLE*,ACCESS_MASK,ACCESS_MASK,OBJECT_ATTRIBUTES*,OBJECT_ATTRIBUTES*,ULONG,ULONG,RTL_USER_PROCESS_PARAMETERS*,ULONG*,ULONG*);
        NTSTATUS PatchNtCreateUserProcess(
            HANDLE*                         ProcessHandle,
            HANDLE*                         ThreadHandle,
            ACCESS_MASK                     ProcessDesiredAccess,
            ACCESS_MASK                     ThreadDesiredAccess,
            OBJECT_ATTRIBUTES*              ProcessObjectAttributes,
            OBJECT_ATTRIBUTES*              ThreadObjectAttributes,
            ULONG                           ProcessFlags,
            ULONG                           ThreadFlags,
            RTL_USER_PROCESS_PARAMETERS*    ProcessParameters,
            ULONG*                          CreateInfo,
            ULONG*                          AttributeList
        ) {
            TypeNtCreateUserProcess function = reinterpret_cast<TypeNtCreateUserProcess>(original( (PatchFunction)PatchNtCreateUserProcess));
            NTSTATUS result = function( ProcessHandle, ThreadHandle, ProcessDesiredAccess, ThreadDesiredAccess, ProcessObjectAttributes, ThreadObjectAttributes, ProcessFlags, ThreadFlags, ProcessParameters, CreateInfo, AttributeList );
            if (monitorLog( PatchExecution )) monitorLog() << "MonitorProcessesAndThreads - NtCreateUserProcess( ... ) -> " << result << record;
            return result;
        }

    }

    void registerProcessesAndThreads() {
        registerPatch( "CreateProcessA", (PatchFunction)PatchCreateProcessA );
        registerPatch( "CreateProcessW", (PatchFunction)PatchCreateProcessW );
        registerPatch( "CreateProcessAsUserA", (PatchFunction)PatchCreateProcessAsUserA );
        registerPatch( "CreateProcessAsUserW", (PatchFunction)PatchCreateProcessAsUserW );
        registerPatch( "CreateProcessWithLogonW", (PatchFunction)PatchCreateProcessWithLogonW );
        registerPatch( "CreateProcessWithTokenW", (PatchFunction)PatchCreateProcessWithTokenW );
        registerPatch( "ExitProcess", (PatchFunction)PatchExitProcess );
        registerPatch( "TerminateProcess", (PatchFunction)PatchTerminateProcess );
        registerPatch( "NtCreateProcess", (PatchFunction)PatchNtCreateUserProcess );
        registerPatch( "NtCreateProcessEx", (PatchFunction)PatchNtCreateUserProcess );
        registerPatch( "NtCreateUserProcess", (PatchFunction)PatchNtCreateUserProcess );
        registerPatch( "LoadLibraryA", (PatchFunction)PatchLoadLibraryA );
        registerPatch( "LoadLibraryW", (PatchFunction)PatchLoadLibraryW );
        registerPatch( "LoadLibraryExA", (PatchFunction)PatchLoadLibraryExA );
        registerPatch( "LoadLibraryExW", (PatchFunction)PatchLoadLibraryExW );
    }
    void unregisterProcessCreation() {
        unregisterPatch( "CreateProcessA" );
        unregisterPatch( "CreateProcessW" );
        unregisterPatch( "CreateProcessAsUserA" );
        unregisterPatch( "CreateProcessAsUserW" );
        unregisterPatch( "CreateProcessWithLogonW" );
        unregisterPatch( "CreateProcessWithTokenW" );
        unregisterPatch( "ExitProcess" );
        unregisterPatch( "TerminateProcess" );
        unregisterPatch( "NtCreateProcess" );
        unregisterPatch( "NtCreateProcessEx" );
        unregisterPatch( "NtCreateUserProcess" );
        unregisterPatch( "LoadLibraryA" );
        unregisterPatch( "LoadLibraryW" );
        unregisterPatch( "LoadLibraryExA" );
        unregisterPatch( "LoadLibraryExW" );
    }  
}