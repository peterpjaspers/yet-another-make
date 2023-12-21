#include "MonitorThreadsAndProcesses.h"
#include "Monitor.h"
#include "Patch.h"
#include "Log.h"

#include <windows.h>
#include <winternl.h>
#include <psapi.h>

#pragma comment(lib, "Advapi32")

// ToDo: Process adminstration for use by application

using namespace std;

namespace AccessMonitor {

    namespace {

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
            BOOL result = function( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (logging( Terse )) log() << "MonitorProcessesAndThreads - CreateProcessA( " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << endLine;
            return result;
        }
        typedef BOOL(*TypeCreateProcessW)(LPCWSTR,LPWSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
        BOOL PatchCreateProcessW(
            LPCWSTR                lpApplicationName,
            LPWSTR                 lpCommandLine,
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
            BOOL result = function( lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (logging( Terse )) log() << "MonitorProcessesAndThreads - CreateProcessW( " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << endLine;
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
            BOOL result = function( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (logging( Terse )) log() << "MonitorProcessesAndThreads - CreateProcessAsUserA( ... , " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << endLine;
            return result;
        }
        typedef BOOL(*TypeCreateProcessAsUserW)(HANDLE,LPCWSTR,LPWSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
        BOOL PatchCreateProcessAsUserW(
            HANDLE                hToken,
            LPCWSTR                lpApplicationName,
            LPWSTR                 lpCommandLine,
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
            BOOL result = function( hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (logging( Terse )) log() << "MonitorProcessesAndThreads - CreateProcessAsUserW( ... , " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << endLine;
            return result;
        }
        typedef BOOL(*TypeCreateProcessWithLogonW)(LPCWSTR,LPCWSTR,LPCWSTR,DWORD,LPCWSTR,LPWSTR,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
        BOOL PatchCreateProcessWithLogonW(
            LPCWSTR               lpUsername,
            LPCWSTR               lpDomain,
            LPCWSTR               lpPassword,
            DWORD                 dwLogonFlags,
            LPCWSTR                lpApplicationName,
            LPWSTR                 lpCommandLine,
            DWORD                 dwCreationFlags,
            LPVOID                lpEnvironment,
            LPCSTR                lpCurrentDirectory,
            LPSTARTUPINFOA        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            TypeCreateProcessWithLogonW function = reinterpret_cast<TypeCreateProcessWithLogonW>(original( (PatchFunction)PatchCreateProcessAsUserW));
            BOOL result = function( lpUsername, lpDomain, lpPassword, dwLogonFlags, lpApplicationName, lpCommandLine, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (logging( Terse )) log() << "MonitorProcessesAndThreads - CreateProcessWithLogonW( ... , " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << endLine;
            return result;
        }
        typedef  BOOL(*TypeCreateProcessWithTokenW)(HANDLE,DWORD,LPCWSTR,LPWSTR,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
        BOOL PatchCreateProcessWithTokenW(
            HANDLE                hToken,
            DWORD                 swLogonFlags,
            LPCWSTR                lpApplicationName,
            LPWSTR                 lpCommandLine,
            DWORD                 dwCreationFlags,
            LPVOID                lpEnvironment,
            LPCSTR                lpCurrentDirectory,
            LPSTARTUPINFOA        lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ) {
            TypeCreateProcessWithTokenW function = reinterpret_cast<TypeCreateProcessWithTokenW>(original( (PatchFunction)PatchCreateProcessAsUserW));
            BOOL result = function( hToken, swLogonFlags, lpApplicationName, lpCommandLine, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation );
            if (logging( Terse )) log() << "MonitorProcessesAndThreads - CreateProcessWithTokenW( ... , " << lpApplicationName << ", " << lpCommandLine << ", ... ) -> " << result << endLine;
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
            NTSTATUS result = function( ProcessHandle, ThreadHandle, ProcessDesiredAccess,ThreadDesiredAccess, ProcessObjectAttributes, ThreadObjectAttributes, ProcessFlags, ThreadFlags, ProcessParameters, CreateInfo, AttributeList );
            if (logging( Terse )) log() << "MonitorProcessesAndThreads - NtCreateUserProcess( ... ) -> " << result << endLine;
            return result;
            
        }
        typedef void(*TypeExitThread)(DWORD);
        void PatchExitThread(
            DWORD dwExitCode
        ) {
            TypeExitThread function = reinterpret_cast<TypeExitThread>(original( (PatchFunction)PatchExitThread));
            if (logging( Terse )) log() << "MonitorProcessesAndThreads - ExitThread( " << dwExitCode << " )" << endLine;
            function( dwExitCode );
        }
        typedef void(*TypeExitProcess)(DWORD);
        void PatchExitProcess(
            UINT uExitCode
        ) {
            TypeExitProcess function = reinterpret_cast<TypeExitProcess>(original( (PatchFunction)PatchExitProcess));
            if (logging( Terse )) log() << "MonitorProcessesAndThreads - ExitProcess( " << uExitCode << " )" << endLine;
            function( uExitCode );
        }
    }

    void registerProcessesAndThreads() {
        registerPatch( "CreateProcessA", (PatchFunction)PatchCreateProcessA );
        registerPatch( "CreateProcessW", (PatchFunction)PatchCreateProcessW );
        registerPatch( "CreateProcessAsUserA", (PatchFunction)PatchCreateProcessAsUserA );
        registerPatch( "CreateProcessAsUserW", (PatchFunction)PatchCreateProcessAsUserW );
        registerPatch( "CreateProcessWithLogonW", (PatchFunction)PatchCreateProcessWithLogonW );
        registerPatch( "CreateProcessWithTokenW", (PatchFunction)PatchCreateProcessWithTokenW );
        registerPatch( "NtCreateUserProcess", (PatchFunction)PatchNtCreateUserProcess );
        registerPatch( "ExitThread", (PatchFunction)PatchExitThread );
        registerPatch( "ExitProcess", (PatchFunction)PatchExitProcess );
    }
    void unregisterProcessCreation() {
        unregisterPatch( "CreateProcessA" );
        unregisterPatch( "CreateProcessW" );
        unregisterPatch( "CreateProcessAsUserA" );
        unregisterPatch( "CreateProcessAsUserW" );
        unregisterPatch( "CreateProcessWithLogonW" );
        unregisterPatch( "CreateProcessWithTokenW" );
        unregisterPatch( "NtCreateUserProcess" );
        unregisterPatch( "ExitThread" );
        unregisterPatch( "ExitProcess" );
    }
}