#include "MonitorFiles.h"
#include "FileAccess.h"
#include "MonitorLogging.h"
#include "Patch.h"

#include <windows.h>
#include <winternl.h>
#include <filesystem>
// #include <Ntstatus.h>

using namespace std;
using namespace std::filesystem;

// Windows Overlay Filter (Wof) functions are not implemented.
// Windows on Windows (WoW) functions are not implemented.
// Lempel-Ziv (LZ) functions are not implemented.

// ToDo: Add function calling convention to all externals
// ToDo: Last write time on directories
// ToDo: Error reporting in monitor logging

// ToDo: Decide whether or not to track following Windows features:
// Iterate over all hard links to a file: Do we kneed to track this? (FindFirstFileName, FindFirstFileNameTransacted)
// Do we need to track compressed file storage? Currently not supported.
// Do we need to track encrypted file storage? Currently not supported. Also not (really) compatible with transactions.

namespace AccessMonitor {

    namespace {

        FileAccessMode requestedAccessMode( DWORD desiredAccess );
        FileAccessMode requestedAccessMode( UINT desiredAccess );

        wstring fileAccess( const string& fileName, FileAccessMode mode );
        wstring fileAccess( const wstring& fileName, FileAccessMode mode );
        wstring fileAccess( HANDLE handle, FileAccessMode mode = AccessNone );
        // wstring fileAccess( HANDLE handle, const OBJECT_ATTRIBUTES* attributes );

        inline uint64_t handleCode( HANDLE handle ) { return reinterpret_cast<uint64_t>( handle ); }

        inline wostream& debugMessage( const char* function ) {
            return debugRecord() << L"MonitorFiles - " << function << "( ";
        }
        inline wostream& debugMessage( const char* function, bool success ) {
            if (!success) debugRecord() << L"MonitorFiles - " << function << L" failed with error : " << GetLastErrorString() << record;
            return debugMessage( function );
        }
        inline wostream& debugMessage( const char* function, HANDLE handle ) {
            return debugMessage( function, (handle != INVALID_HANDLE_VALUE) );
        }

        typedef BOOL(*TypeCreateDirectoryA)(LPCSTR,LPSECURITY_ATTRIBUTES);
        BOOL PatchCreateDirectoryA(
            LPCSTR                pathName,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto function = reinterpret_cast<TypeCreateDirectoryA>(original( (PatchFunction)PatchCreateDirectoryA ));
            BOOL created = function( pathName, securityAttributes );
            if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryA", created ) << pathName << L", ... )" << record;
            fileAccess( pathName, AccessWrite );
            return created;
        }
        typedef BOOL(*TypeCreateDirectoryW)(LPCWSTR,LPSECURITY_ATTRIBUTES);
        BOOL PatchCreateDirectoryW(
            LPCWSTR                pathName,
            LPSECURITY_ATTRIBUTES  securityAttributes
        ) {
            auto function = reinterpret_cast<TypeCreateDirectoryW>(original( (PatchFunction)PatchCreateDirectoryW ));
            BOOL created = function( pathName, securityAttributes );
            if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryW", created ) << pathName << L", ... )" << record;
            fileAccess( pathName, AccessWrite );
            return created;
        }
        typedef BOOL(*TypeCreateDirectoryExA)(LPCSTR,LPCSTR,LPSECURITY_ATTRIBUTES);
        BOOL PatchCreateDirectoryExA(
            LPCSTR                templateDirectory,
            LPCSTR                newDirectory,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto function = reinterpret_cast<TypeCreateDirectoryExA>(original( (PatchFunction)PatchCreateDirectoryExA ));
            BOOL created = function( templateDirectory, newDirectory, securityAttributes );
            if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryExA", created ) << newDirectory << L", ... )" << record;
            fileAccess( newDirectory, AccessWrite );
            return created;
        }
        typedef BOOL(*TypeCreateDirectoryExW)(LPCWSTR,LPCWSTR,LPSECURITY_ATTRIBUTES);
        BOOL PatchCreateDirectoryExW(
            LPCWSTR                templateDirectory,
            LPCWSTR                newDirectory,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto function = reinterpret_cast<TypeCreateDirectoryExW>(original( (PatchFunction)PatchCreateDirectoryExW ));
            BOOL created = function( templateDirectory, newDirectory, securityAttributes );
            if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryExA", created ) << newDirectory << L", ... )" << record;
            fileAccess( newDirectory, AccessWrite );
            return created;
        }
        typedef BOOL(*TypeRemoveDirectoryA)(LPCSTR);
        BOOL PatchRemoveDirectoryA(
            LPCSTR pathName
        ) {
            auto function = reinterpret_cast<TypeRemoveDirectoryA>(original( (PatchFunction)PatchRemoveDirectoryA ));
            BOOL removed = function( pathName );
            if (debugLog( PatchExecution )) debugMessage( "RemoveDirectoryA", removed ) << pathName << L", ... )" << record;
            fileAccess( pathName, AccessDelete );
            return removed;
        }
        typedef BOOL(*TypeRemoveDirectoryW)(LPCWSTR);
        BOOL PatchRemoveDirectoryW(
            LPCWSTR pathName
        ) {
            auto function = reinterpret_cast<TypeRemoveDirectoryW>(original( (PatchFunction)PatchRemoveDirectoryW ));
            BOOL removed = function( pathName );
            if (debugLog( PatchExecution )) debugMessage( "RemoveDirectoryW", removed ) << pathName << L", ... )" << record;
            fileAccess( pathName, AccessDelete );
            return removed;
        }
        typedef HANDLE(*TypeCreateFileA)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
        HANDLE PatchCreateFileA(
            LPCSTR                fileName,
            DWORD                 dwDesiredAccess,
            DWORD                 dwShareMode,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            DWORD                 dwCreationDisposition,
            DWORD                 dwFlagsAndAttributes,
            HANDLE                hTemplateFile
        ) {
            auto function = reinterpret_cast<TypeCreateFileA>(original( (PatchFunction)PatchCreateFileA ));
            HANDLE handle = function( fileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile );
            if (debugLog( PatchExecution )) debugMessage( "CreateFileA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            return handle;
        }
        typedef HANDLE(*TypeCreateFileW)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
        HANDLE PatchCreateFileW(
            LPCWSTR               fileName,
            DWORD                 dwDesiredAccess,
            DWORD                 dwShareMode,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            DWORD                 dwCreationDisposition,
            DWORD                 dwFlagsAndAttributes,
            HANDLE                hTemplateFile
        ) {
            auto function = reinterpret_cast<TypeCreateFileW>(original( (PatchFunction)PatchCreateFileW ));
            HANDLE handle = function( fileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile );
            if (debugLog( PatchExecution )) debugMessage( "CreateFileW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            return handle;
        }
        typedef HANDLE(*TypeCreateFileTransactedA)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE,HANDLE,PUSHORT,PVOID);
        HANDLE PatchCreateFileTransactedA(
            LPCSTR                fileName,
            DWORD                 dwDesiredAccess,
            DWORD                 dwShareMode,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            DWORD                 dwCreationDisposition,
            DWORD                 dwFlagsAndAttributes,
            HANDLE                hTemplateFile,
            HANDLE                hTransaction,
            PUSHORT               pusMiniVersion,
            PVOID                 lpExtendedParameter
        ) {
            auto function = reinterpret_cast<TypeCreateFileTransactedA>(original( (PatchFunction)PatchCreateFileTransactedA ));
            HANDLE handle = function( fileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, hTransaction, pusMiniVersion, lpExtendedParameter );
            if (debugLog( PatchExecution )) debugMessage( "CreateFileTransactedA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            return handle;
        }
        typedef HANDLE(*TypeCreateFileTransactedW)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE,HANDLE,PUSHORT,PVOID);
        HANDLE PatchCreateFileTransactedW(
            LPCWSTR                fileName,
            DWORD                 dwDesiredAccess,
            DWORD                 dwShareMode,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            DWORD                 dwCreationDisposition,
            DWORD                 dwFlagsAndAttributes,
            HANDLE                hTemplateFile,
            HANDLE                hTransaction,
            PUSHORT               pusMiniVersion,
            PVOID                 lpExtendedParameter
        ) {
            auto function = reinterpret_cast<TypeCreateFileTransactedW>(original( (PatchFunction)PatchCreateFileTransactedW ));
            HANDLE handle = function( fileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, hTransaction, pusMiniVersion, lpExtendedParameter );
            if (debugLog( PatchExecution )) debugMessage( "CreateFileTransactedW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            return handle;
        }
        typedef HANDLE(*TypeCreateFile2)(LPCWSTR,DWORD,DWORD,DWORD,LPCREATEFILE2_EXTENDED_PARAMETERS);
        HANDLE PatchCreateFile2(
            LPCWSTR                           fileName,
            DWORD                             dwDesiredAccess,
            DWORD                             dwShareMode,
            DWORD                             dwCreationDisposition,
            LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams
        ) {
            auto function = reinterpret_cast<TypeCreateFile2>(original( (PatchFunction)PatchCreateFile2 ));
            HANDLE handle = function( fileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams );
            if (debugLog( PatchExecution )) debugMessage( "CreateFile2", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            return handle;
        }
        typedef HANDLE(*TypeReOpenFile)(HANDLE,DWORD,DWORD,DWORD);
        HANDLE PatchReOpenFile(
            HANDLE hOriginalFile,
            DWORD  dwDesiredAccess,
            DWORD  dwShareMode,
            DWORD  dwFlagsAndAttributes
        ) {
            auto function = reinterpret_cast<TypeReOpenFile>(original( (PatchFunction)PatchReOpenFile ));
            HANDLE handle = function( hOriginalFile, dwDesiredAccess, dwShareMode, dwFlagsAndAttributes );
            auto fileName = fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            if (debugLog( PatchExecution )) debugMessage( "ReOpenFile", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            return handle;
        }
        typedef HFILE(*TypeOpenFile)(LPCSTR,LPOFSTRUCT,UINT);
        HFILE PatchOpenFile(
            LPCSTR     fileName,
            LPOFSTRUCT lpReOpenBuff,
            UINT       uStyle
        ) {
            auto function = reinterpret_cast<TypeOpenFile>(original( (PatchFunction)PatchOpenFile ));
            HFILE handle = function( fileName, lpReOpenBuff, uStyle );
            if (debugLog( PatchExecution )) debugMessage( "OpenFile", (handle != HFILE_ERROR) ) << fileName << L", ... ) -> " << handle << record;
            fileAccess( fileName, requestedAccessMode( uStyle ) );
            return handle;

        }
        typedef BOOL(*TypeDeleteFileA)(LPCSTR);
        BOOL PatchDeleteFileA( LPCSTR fileName ) {
            auto function = reinterpret_cast<TypeDeleteFileA>(original( (PatchFunction)PatchDeleteFileA ));
            BOOL deleted = function( fileName );
            if (debugLog( PatchExecution )) debugMessage( "DeleteFileA", deleted ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessDelete );
            return deleted;
        }
        typedef BOOL(*TypeDeleteFileW)(LPCWSTR);
        BOOL PatchDeleteFileW( LPCWSTR fileName ) {
            auto function = reinterpret_cast<TypeDeleteFileW>(original( (PatchFunction)PatchDeleteFileW ));
            BOOL deleted = function( fileName );
            if (debugLog( PatchExecution )) debugMessage( "DeleteFileW", deleted ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessDelete );
            return deleted;
        }
        typedef BOOL(*TypeDeleteFileTransactedA)(LPCSTR,HANDLE);
        BOOL PatchDeleteFileTransactedA( LPCSTR fileName, HANDLE hTransaction ) {
            auto function = reinterpret_cast<TypeDeleteFileTransactedA>(original( (PatchFunction)PatchDeleteFileTransactedA ));
            BOOL deleted = function( fileName, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "DeleteFileTransactedA", deleted ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessDelete );
            return deleted;
        }
        typedef BOOL(*TypeDeleteFileTransactedW)(LPCWSTR,HANDLE);
        BOOL PatchDeleteFileTransactedW( LPCWSTR fileName, HANDLE hTransaction ) {
            auto function = reinterpret_cast<TypeDeleteFileTransactedW>(original( (PatchFunction)PatchDeleteFileTransactedW ));
            BOOL deleted = function( fileName, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "DeleteFileTransactedW", deleted ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessDelete );
            return deleted;
        }
        typedef BOOL(*TypeCopyFileA)(LPCSTR,LPCSTR,BOOL);
        BOOL PatchCopyFileA(
            LPCSTR existingFileName,
            LPCSTR newFileName,
            BOOL   failIfExists
        ) {
            auto function = reinterpret_cast<TypeCopyFileA>(original( (PatchFunction)PatchCopyFileA ));
            BOOL copied = function( existingFileName, newFileName, failIfExists );
            if (debugLog( PatchExecution )) debugMessage( "CopyFileA", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessRead );
            fileAccess( newFileName, AccessWrite );
            return copied;
        }
        typedef BOOL(*TypeCopyFileW)(LPCWSTR,LPCWSTR,BOOL);
        BOOL PatchCopyFileW(
            LPCWSTR existingFileName,
            LPCWSTR newFileName,
            BOOL    failIfExists
        ) {
            auto function = reinterpret_cast<TypeCopyFileW>(original( (PatchFunction)PatchCopyFileW ));
            BOOL copied = function( existingFileName, newFileName, failIfExists );
            if (debugLog( PatchExecution )) debugMessage( "CopyFileW", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessRead);
            fileAccess( newFileName, AccessWrite );
            return copied;
        }
        typedef BOOL(*TypeCopyFileExA)(LPCSTR,LPCSTR,LPPROGRESS_ROUTINE,LPVOID,LPBOOL,DWORD);
        BOOL PatchCopyFileExA(
            LPCSTR             existingFileName,
            LPCSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            LPBOOL             pbCancel,
            DWORD              dwCopyFlags
        ) {
            auto function = reinterpret_cast<TypeCopyFileExA>(original( (PatchFunction)PatchCopyFileExA ));
            BOOL copied = function( existingFileName, newFileName, lpProgressRoutine, lpData, pbCancel, dwCopyFlags );
            if (debugLog( PatchExecution )) debugMessage( "CopyFileExA", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessRead );
            fileAccess( newFileName, AccessWrite );
            return copied;
        }
        typedef BOOL(*TypeCopyFileExW)(LPCWSTR,LPCWSTR,LPPROGRESS_ROUTINE,LPVOID,LPBOOL,DWORD);
        BOOL PatchCopyFileExW(
            LPCWSTR             existingFileName,
            LPCWSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            LPBOOL             pbCancel,
            DWORD              dwCopyFlags
        ) {
            auto function = reinterpret_cast<TypeCopyFileExW>(original( (PatchFunction)PatchCopyFileExW ));
            BOOL copied = function( existingFileName, newFileName, lpProgressRoutine, lpData, pbCancel, dwCopyFlags );
            if (debugLog( PatchExecution )) debugMessage( "CopyFileExW", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessRead );
            fileAccess( newFileName, AccessWrite );
            return copied;
        }
        typedef BOOL(*TypeCopyFileTransactedA)(LPCSTR,LPCSTR,LPPROGRESS_ROUTINE,LPVOID,LPBOOL,DWORD,HANDLE);
        BOOL PatchCopyFileTransactedA(
            LPCSTR             existingFileName,
            LPCSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            LPBOOL             pbCancel,
            DWORD              dwCopyFlags,
            HANDLE             hTransaction
        ) {
            auto function = reinterpret_cast<TypeCopyFileTransactedA>(original( (PatchFunction)PatchCopyFileTransactedA ));
            BOOL copied = function( existingFileName, newFileName, lpProgressRoutine,lpData, pbCancel, dwCopyFlags, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "CopyFileTransactedA", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessRead );
            fileAccess( newFileName, AccessWrite );
            return copied;
        }
        typedef BOOL(*TypeCopyFileTransactedW)(LPCWSTR,LPCWSTR,LPPROGRESS_ROUTINE,LPVOID,LPBOOL,DWORD,HANDLE);
        BOOL PatchCopyFileTransactedW(
            LPCWSTR            existingFileName,
            LPCWSTR            newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            LPBOOL             pbCancel,
            DWORD              dwCopyFlags,
            HANDLE             hTransaction
        ) {
            auto function = reinterpret_cast<TypeCopyFileTransactedW>(original( (PatchFunction)PatchCopyFileTransactedW ));
            BOOL copied = function( existingFileName, newFileName, lpProgressRoutine,lpData, pbCancel, dwCopyFlags, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "CopyFileTransactedW", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessRead);
            fileAccess( newFileName, AccessWrite );
            return copied;
        }
        typedef HRESULT(*TypeCopyFile2)(PCWSTR,PCWSTR,COPYFILE2_EXTENDED_PARAMETERS*);
        HRESULT PatchCopyFile2(
            PCWSTR                         existingFileName,
            PCWSTR                         newFileName,
            COPYFILE2_EXTENDED_PARAMETERS* pExtendedParameters
        ) {
            auto function = reinterpret_cast<TypeCopyFile2>(original( (PatchFunction)PatchCopyFile2 ));
            BOOL copied = function( existingFileName, newFileName, pExtendedParameters );
            if (debugLog( PatchExecution )) debugMessage( "CopyFile2", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessRead );
            fileAccess( newFileName, AccessWrite );
            return copied;
        }
        typedef BOOL(*TypeReplaceFileA)(LPCSTR,LPCSTR,LPCSTR,DWORD,LPVOID,LPVOID);
        BOOL PatchReplaceFileA(
            LPCSTR  lpReplacedFileName,
            LPCSTR  lpReplacementFileName,
            LPCSTR  lpBackupFileName,
            DWORD   dwReplaceFlags,
            LPVOID  lpExclude,
            LPVOID  lpReserved
        ) {
            auto function = reinterpret_cast<TypeReplaceFileA>(original( (PatchFunction)PatchReplaceFileA ));
            BOOL replaced = function( lpReplacedFileName, lpReplacementFileName, lpBackupFileName, dwReplaceFlags, lpExclude, lpReserved );
            if (debugLog( PatchExecution )) debugMessage( "ReplaceFileA", replaced ) << lpReplacedFileName << L", " << lpReplacementFileName << L", ... )" << record;
            fileAccess( lpReplacementFileName, AccessRead );
            fileAccess( lpReplacedFileName, AccessWrite );
            if (lpBackupFileName!= nullptr) fileAccess( lpBackupFileName, AccessWrite );
            return replaced;
        }
        typedef BOOL(*TypeReplaceFileW)(LPCWSTR,LPCWSTR,LPCWSTR,DWORD,LPVOID,LPVOID);
        BOOL PatchReplaceFileW(
            LPCWSTR lpReplacedFileName,
            LPCWSTR lpReplacementFileName,
            LPCWSTR lpBackupFileName,
            DWORD   dwReplaceFlags,
            LPVOID  lpExclude,
            LPVOID  lpReserved
        ) {
            auto function = reinterpret_cast<TypeReplaceFileW>(original( (PatchFunction)PatchReplaceFileW ));
            BOOL replaced = function( lpReplacedFileName, lpReplacementFileName, lpBackupFileName, dwReplaceFlags, lpExclude, lpReserved );
            if (debugLog( PatchExecution )) debugMessage( "ReplaceFileW", replaced ) << lpReplacedFileName << L", " << lpReplacementFileName << L", ... )" << record;
            fileAccess( lpReplacementFileName, AccessRead );
            fileAccess( lpReplacedFileName, AccessWrite );
            if (lpBackupFileName!= nullptr) fileAccess( lpBackupFileName, AccessWrite );
            return replaced;
        }
        typedef BOOL(*TypeMoveFileA)(LPCSTR,LPCSTR);
        BOOL PatchMoveFileA(
            LPCSTR existingFileName,
            LPCSTR newFileName
        ) {
            auto function = reinterpret_cast<TypeMoveFileA>(original( (PatchFunction)PatchMoveFileA ));
            BOOL moved = function( existingFileName, newFileName );
            if (debugLog( PatchExecution )) debugMessage( "MoveFileA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessDelete );
            fileAccess( newFileName, AccessWrite );
            return moved;
        }       
        typedef BOOL(*TypeMoveFileW)(LPCWSTR,LPCWSTR);
        BOOL PatchMoveFileW(
            LPCWSTR existingFileName,
            LPCWSTR newFileName
        ) {
            auto function = reinterpret_cast<TypeMoveFileW>(original( (PatchFunction)PatchMoveFileW ));
            BOOL moved = function( existingFileName, newFileName );
            if (debugLog( PatchExecution )) debugMessage( "MoveFileW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessDelete );
            fileAccess( newFileName, AccessWrite );
            return moved;
        }
        typedef BOOL(*TypeMoveFileExA)(LPCSTR,LPCSTR,DWORD);
        BOOL PatchMoveFileExA(
            LPCSTR  existingFileName,
            LPCSTR  newFileName,
            DWORD   flags
        ) {
            auto function = reinterpret_cast<TypeMoveFileExA>(original( (PatchFunction)PatchMoveFileExA ));
            BOOL moved = function( existingFileName, newFileName, flags );
            if (debugLog( PatchExecution )) debugMessage( "MoveFileExA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessDelete );
            fileAccess( newFileName, AccessWrite );
            return moved;
        }       
        typedef BOOL(*TypeMoveFileExW)(LPCWSTR,LPCWSTR,DWORD);
        BOOL PatchMoveFileExW(
            LPCWSTR existingFileName,
            LPCWSTR newFileName,
            DWORD   flags
        ) {
            auto function = reinterpret_cast<TypeMoveFileExW>(original( (PatchFunction)PatchMoveFileExW ));
            BOOL moved = function( existingFileName, newFileName, flags );
            if (debugLog( PatchExecution )) debugMessage( "MoveFileExW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessDelete );
            fileAccess( newFileName, AccessWrite );
            return moved;
        }       
        typedef BOOL(*TypeMoveFileWithProgressA)(LPCSTR,LPCSTR,LPPROGRESS_ROUTINE,LPVOID,DWORD);
        BOOL PatchMoveFileWithProgressA(
            LPCSTR             existingFileName,
            LPCSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            DWORD              dwFlags
        ) {
            auto function = reinterpret_cast<TypeMoveFileWithProgressA>(original( (PatchFunction)PatchMoveFileWithProgressA ));
            BOOL moved = function( existingFileName, newFileName, lpProgressRoutine, lpData, dwFlags );
            if (debugLog( PatchExecution )) debugMessage( "MoveFileWithProgressA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessDelete );
            fileAccess( newFileName, AccessWrite );
            // ToDo: Record last write time on newFileName (wait until copy coplete!)
            return moved;
        }
        typedef BOOL(*TypeMoveFileWithProgressW)(LPCWSTR,LPCWSTR,LPPROGRESS_ROUTINE,LPVOID,DWORD);
        BOOL PatchMoveFileWithProgressW(
            LPCWSTR             existingFileName,
            LPCWSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            DWORD              dwFlags
        ) {
            auto function = reinterpret_cast<TypeMoveFileWithProgressW>(original( (PatchFunction)PatchMoveFileWithProgressW ));
            BOOL moved = function( existingFileName, newFileName, lpProgressRoutine, lpData, dwFlags );
            if (debugLog( PatchExecution )) debugMessage( "MoveFileWithProgressW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessDelete );
            fileAccess( newFileName, AccessWrite );
            // ToDo: Record last write time on newFileName (wait until copy complete!)
            return moved;
        }
        typedef BOOL(*TypeMoveFileWithProgressTransactedA)(LPCSTR,LPCSTR,LPPROGRESS_ROUTINE,LPVOID,DWORD,HANDLE);
        BOOL PatchMoveFileWithProgressTransactedA(
            LPCSTR             existingFileName,
            LPCSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            DWORD              dwFlags,
            HANDLE             hTransaction
        ) {
            auto function = reinterpret_cast<TypeMoveFileWithProgressTransactedA>(original( (PatchFunction)PatchMoveFileWithProgressTransactedA ));
            BOOL moved = function( existingFileName, newFileName, lpProgressRoutine, lpData, dwFlags, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "PatchMoveFileWithProgressTransactedA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessDelete );
            fileAccess( newFileName, AccessWrite );
            return moved;
        }
        typedef BOOL(*TypeMoveFileWithProgressTransactedW)(LPCWSTR,LPCWSTR,LPPROGRESS_ROUTINE,LPVOID,DWORD,HANDLE);
        BOOL PatchMoveFileWithProgressTransactedW(
            LPCWSTR             existingFileName,
            LPCWSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            DWORD              dwFlags,
            HANDLE             hTransaction
        ) {
            auto function = reinterpret_cast<TypeMoveFileWithProgressTransactedW>(original( (PatchFunction)PatchMoveFileWithProgressTransactedW ));
            BOOL moved = function( existingFileName, newFileName, lpProgressRoutine, lpData, dwFlags, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "PatchMoveFileWithProgressTransactedW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
            fileAccess( existingFileName, AccessDelete );
            fileAccess( newFileName, AccessWrite );
            return moved;
        }
        typedef HANDLE(*TypeFindFirstFileA)(LPCSTR,LPWIN32_FIND_DATAA);
        HANDLE PatchFindFirstFileA(
            LPCSTR             fileName,
            LPWIN32_FIND_DATAA lpFindFileData
        ) {
            auto function = reinterpret_cast<TypeFindFirstFileA>(original( (PatchFunction)PatchFindFirstFileA ));
            HANDLE handle = function( fileName, lpFindFileData );
            if (debugLog( PatchExecution )) debugMessage( "FindFirstFileA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( fileName, AccessRead );
            return handle;
        }        
        typedef HANDLE(*TypeFindFirstFileW)(LPCWSTR,LPWIN32_FIND_DATAW);
        HANDLE PatchFindFirstFileW(
            LPCWSTR            fileName,
            LPWIN32_FIND_DATAW lpFindFileData
        ) {
            auto function = reinterpret_cast<TypeFindFirstFileW>(original( (PatchFunction)PatchFindFirstFileW ));
            HANDLE handle = function( fileName, lpFindFileData );
            if (debugLog( PatchExecution )) debugMessage( "FindFirstFileW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( fileName, AccessRead );
            return handle;
        }
        typedef HANDLE(*TypeFindFirstFileExA)(LPCSTR,FINDEX_INFO_LEVELS,LPVOID,FINDEX_SEARCH_OPS,LPVOID,DWORD);
        HANDLE PatchFindFirstFileExA(
            LPCSTR             fileName,
            FINDEX_INFO_LEVELS fInfoLevelId,
            LPVOID             lpFindFileData,
            FINDEX_SEARCH_OPS  fSearchOp,
            LPVOID             lpSearchFilter,
            DWORD              dwAdditionalFlags
        ) {
            auto function = reinterpret_cast<TypeFindFirstFileExA>(original( (PatchFunction)PatchFindFirstFileExA ));
            HANDLE handle = function( fileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags );
            if (debugLog( PatchExecution )) debugMessage( "FindFirstFileExA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( fileName, AccessRead );
            return handle;
        }
        typedef HANDLE(*TypeFindFirstFileExW)(LPCWSTR,FINDEX_INFO_LEVELS,LPVOID,FINDEX_SEARCH_OPS,LPVOID,DWORD);
        HANDLE PatchFindFirstFileExW(
            LPCWSTR            fileName,
            FINDEX_INFO_LEVELS fInfoLevelId,
            LPVOID             lpFindFileData,
            FINDEX_SEARCH_OPS  fSearchOp,
            LPVOID             lpSearchFilter,
            DWORD              dwAdditionalFlags
        ) {
            auto function = reinterpret_cast<TypeFindFirstFileExW>(original( (PatchFunction)PatchFindFirstFileExW ));
            HANDLE handle = function( fileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags );
            if (debugLog( PatchExecution )) debugMessage( "FindFirstFileExW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( fileName, AccessRead );
            return handle;
        }
        typedef HANDLE(*TypeFindFirstFileTransactedA)(LPCSTR,FINDEX_INFO_LEVELS,LPVOID,FINDEX_SEARCH_OPS,LPVOID,DWORD,HANDLE);
        HANDLE PatchFindFirstFileTransactedA(
            LPCSTR             fileName,
            FINDEX_INFO_LEVELS fInfoLevelId,
            LPVOID             lpFindFileData,
            FINDEX_SEARCH_OPS  fSearchOp,
            LPVOID             lpSearchFilter,
            DWORD              dwAdditionalFlags,
            HANDLE             hTransaction
        ) {
            auto function = reinterpret_cast<TypeFindFirstFileTransactedA>(original( (PatchFunction)PatchFindFirstFileTransactedA ));
            HANDLE handle = function( fileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "FindFirstFileTransactedA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( fileName, AccessRead );
            return handle;
        }
        typedef HANDLE(*TypeFindFirstFileTransactedW)(LPCWSTR,FINDEX_INFO_LEVELS,LPVOID,FINDEX_SEARCH_OPS,LPVOID,DWORD,HANDLE);
        HANDLE PatchFindFirstFileTransactedW(
            LPCWSTR            fileName,
            FINDEX_INFO_LEVELS fInfoLevelId,
            LPVOID             lpFindFileData,
            FINDEX_SEARCH_OPS  fSearchOp,
            LPVOID             lpSearchFilter,
            DWORD              dwAdditionalFlags,
            HANDLE             hTransaction
        ) {
            auto function = reinterpret_cast<TypeFindFirstFileTransactedW>(original( (PatchFunction)PatchFindFirstFileTransactedW ));
            HANDLE handle = function( fileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "FindFirstFileTransactedW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( fileName, AccessRead );
            return handle;
        }
        typedef DWORD(*TypeGetFileAttributesA)(LPCSTR);
        DWORD PatchGetFileAttributesA(
            LPCSTR fileName
        ) {
            auto function = reinterpret_cast<TypeGetFileAttributesA>(original( (PatchFunction)PatchGetFileAttributesA ));
            DWORD attributes = function( fileName );
            if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesA", (attributes != INVALID_FILE_ATTRIBUTES) ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessRead );
            return attributes;
        }
        typedef DWORD(*TypeGetFileAttributesW)(LPCWSTR);
        DWORD PatchGetFileAttributesW(
            LPCWSTR fileName
        ) {
            auto function = reinterpret_cast<TypeGetFileAttributesW>(original( (PatchFunction)PatchGetFileAttributesW ));
            DWORD attributes = function( fileName );
            if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesW", (attributes != INVALID_FILE_ATTRIBUTES) ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessRead );
            return attributes;
        }
        typedef BOOL(*TypeGetFileAttributesExA)(LPCSTR,GET_FILEEX_INFO_LEVELS,LPVOID);
        BOOL PatchGetFileAttributesExA(
            LPCSTR                 fileName,
            GET_FILEEX_INFO_LEVELS fInfoLevelId,
            LPVOID                 lpFileInformation
        ) {
            auto function = reinterpret_cast<TypeGetFileAttributesExA>(original( (PatchFunction)PatchGetFileAttributesExA ));
            DWORD attributes = function( fileName, fInfoLevelId, lpFileInformation );
            if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesExA", (attributes != INVALID_FILE_ATTRIBUTES) ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessRead );
            return attributes;
        }
        typedef BOOL(*TypeGetFileAttributesExW)(LPCWSTR,GET_FILEEX_INFO_LEVELS,LPVOID);
        BOOL PatchGetFileAttributesExW(
            LPCWSTR                 fileName,
            GET_FILEEX_INFO_LEVELS fInfoLevelId,
            LPVOID                 lpFileInformation
        ) {
            auto function = reinterpret_cast<TypeGetFileAttributesExW>(original( (PatchFunction)PatchGetFileAttributesExW ));
            DWORD attributes = function( fileName, fInfoLevelId, lpFileInformation );
            if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesExW", (attributes != INVALID_FILE_ATTRIBUTES) ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessRead );
            return attributes;
        }
        typedef BOOL(*TypeGetFileAttributesTransactedA)(LPCSTR,GET_FILEEX_INFO_LEVELS,LPVOID,HANDLE);
        BOOL PatchGetFileAttributesTransactedA(
            LPCSTR                 fileName,
            GET_FILEEX_INFO_LEVELS fInfoLevelId,
            LPVOID                 lpFileInformation,
            HANDLE                 hTransaction
        ) {
            auto function = reinterpret_cast<TypeGetFileAttributesTransactedA>(original( (PatchFunction)PatchGetFileAttributesTransactedA ));
            DWORD attributes = function( fileName, fInfoLevelId, lpFileInformation, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesTransactedA", (attributes != INVALID_FILE_ATTRIBUTES) ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessRead );
            return attributes;
        }
        typedef BOOL(*TypeGetFileAttributesTransactedW)(LPCWSTR,GET_FILEEX_INFO_LEVELS,LPVOID,HANDLE);
        BOOL PatchGetFileAttributesTransactedW(
            LPCWSTR                 fileName,
            GET_FILEEX_INFO_LEVELS fInfoLevelId,
            LPVOID                 lpFileInformation,
            HANDLE                 hTransaction
        ) {
            auto function = reinterpret_cast<TypeGetFileAttributesTransactedW>(original( (PatchFunction)PatchGetFileAttributesTransactedW ));
            DWORD attributes = function( fileName, fInfoLevelId, lpFileInformation, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesTransactedW", (attributes != INVALID_FILE_ATTRIBUTES) ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessRead );
            return attributes;
        }
        typedef BOOL(*TypeSetFileAttributesA)(LPCSTR,DWORD);
        BOOL PatchSetFileAttributesA(
            LPCSTR fileName,
            DWORD  dwFileAttributes
        ) {
            auto function = reinterpret_cast<TypeSetFileAttributesA>(original( (PatchFunction)PatchSetFileAttributesA ));
            BOOL set = function( fileName, dwFileAttributes );
            if (debugLog( PatchExecution )) debugMessage( "PatchSetFileAttributesA", set ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessWrite );
            return set;
        }
        typedef BOOL(*TypeSetFileAttributesW)(LPCWSTR,DWORD);
        BOOL PatchSetFileAttributesW(
            LPCWSTR fileName,
            DWORD  dwFileAttributes
        ) {
            auto function = reinterpret_cast<TypeSetFileAttributesW>(original( (PatchFunction)PatchSetFileAttributesW ));
            BOOL set = function( fileName, dwFileAttributes );
            if (debugLog( PatchExecution )) debugMessage( "PatchSetFileAttributesW", set ) << fileName << L", ... )" << record;
            fileAccess( fileName, AccessWrite );
            return set;
        }
        typedef BOOL(*TypeCreateHardLinkA)(LPCSTR,LPCSTR,LPSECURITY_ATTRIBUTES);
        BOOL PatchCreateHardLinkA(
            LPCSTR                lpFileName,
            LPCSTR                lpExistingFileName,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes
        ) {
            auto function = reinterpret_cast<TypeCreateHardLinkA>(original( (PatchFunction)PatchCreateHardLinkA ));
            BOOL created = function( lpFileName, lpExistingFileName, lpSecurityAttributes );
            if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkA", created )  << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
            fileAccess( lpFileName, AccessWrite );
            fileAccess( lpExistingFileName, AccessRead );
            return created;
        }
        typedef BOOL(*TypeCreateHardLinkW)(LPCWSTR,LPCWSTR,LPSECURITY_ATTRIBUTES);
        BOOL PatchCreateHardLinkW(
            LPCWSTR                lpFileName,
            LPCWSTR                lpExistingFileName,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes
        ) {
            auto function = reinterpret_cast<TypeCreateHardLinkW>(original( (PatchFunction)PatchCreateHardLinkW ));
            BOOL created = function( lpFileName, lpExistingFileName, lpSecurityAttributes );
            if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkW", created )  << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
            fileAccess( lpFileName, AccessWrite );
            fileAccess( lpExistingFileName, AccessRead );
            return created;
        }
        typedef BOOL(*TypeCreateHardLinkTransactedA)(LPCSTR,LPCSTR,LPSECURITY_ATTRIBUTES,HANDLE);
        BOOL PatchCreateHardLinkTransactedA(
            LPCSTR                lpFileName,
            LPCSTR                lpExistingFileName,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            HANDLE                hTransaction
        ) {
            auto function = reinterpret_cast<TypeCreateHardLinkTransactedA>(original( (PatchFunction)PatchCreateHardLinkTransactedA ));
            BOOL created = function( lpFileName, lpExistingFileName, lpSecurityAttributes, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkTransactedA", created ) << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
            fileAccess( lpFileName, AccessWrite );
            fileAccess( lpExistingFileName, AccessRead );
            return created;
        }
        typedef BOOL(*TypeCreateHardLinkTransactedW)(LPCWSTR,LPCWSTR,LPSECURITY_ATTRIBUTES,HANDLE);
        BOOL PatchCreateHardLinkTransactedW(
            LPCWSTR               lpFileName,
            LPCWSTR               lpExistingFileName,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            HANDLE                hTransaction
        ) {
            auto function = reinterpret_cast<TypeCreateHardLinkTransactedW>(original( (PatchFunction)PatchCreateHardLinkTransactedW ));
            BOOL created = function( lpFileName, lpExistingFileName, lpSecurityAttributes, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkTransactedW", created ) << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
            fileAccess( lpFileName, AccessWrite );
            fileAccess( lpExistingFileName, AccessRead );
            return created;
        }
        typedef BOOLEAN(*TypeCreateSymbolicLinkA)(LPCSTR,LPCSTR,DWORD);
        BOOLEAN PatchCreateSymbolicLinkA(
            LPCSTR lpSymlinkFileName,
            LPCSTR lpTargetFileName,
            DWORD  dwFlags
        ) {
            auto function = reinterpret_cast<TypeCreateSymbolicLinkA>(original( (PatchFunction)PatchCreateSymbolicLinkA ));
            BOOL created = function( lpSymlinkFileName, lpTargetFileName, dwFlags );
            if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkA", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
            fileAccess( lpSymlinkFileName, AccessWrite );
            fileAccess( lpTargetFileName, AccessRead );
            return created;
        }
        typedef BOOLEAN(*TypeCreateSymbolicLinkW)(LPCWSTR,LPCWSTR,DWORD);
        BOOLEAN PatchCreateSymbolicLinkW(
            LPCWSTR lpSymlinkFileName,
            LPCWSTR lpTargetFileName,
            DWORD   dwFlags
        ) {
            auto function = reinterpret_cast<TypeCreateSymbolicLinkW>(original( (PatchFunction)PatchCreateSymbolicLinkW ));
            BOOL created = function( lpSymlinkFileName, lpTargetFileName, dwFlags );
            if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkW", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
            fileAccess( lpSymlinkFileName, AccessWrite );
            fileAccess( lpTargetFileName, AccessRead );
            return created;
        }
        typedef BOOLEAN(*TypeCreateSymbolicLinkTransactedA)(LPCSTR,LPCSTR,DWORD,HANDLE);
        BOOLEAN PatchCreateSymbolicLinkTransactedA(
            LPCSTR lpSymlinkFileName,
            LPCSTR lpTargetFileName,
            DWORD  dwFlags,
            HANDLE hTransaction
        ) {
            auto function = reinterpret_cast<TypeCreateSymbolicLinkTransactedA>(original( (PatchFunction)PatchCreateSymbolicLinkTransactedA ));
            BOOL created = function( lpSymlinkFileName, lpTargetFileName, dwFlags, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkTransactedA", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
            fileAccess( lpSymlinkFileName, AccessWrite );
            fileAccess( lpTargetFileName, AccessRead );
            return created;
        }
        typedef BOOLEAN(*TypeCreateSymbolicLinkTransactedW)(LPCWSTR,LPCWSTR,DWORD,HANDLE);
        BOOLEAN PatchCreateSymbolicLinkTransactedW(
            LPCWSTR lpSymlinkFileName,
            LPCWSTR lpTargetFileName,
            DWORD   dwFlags,
            HANDLE hTransaction
        ) {
            auto function = reinterpret_cast<TypeCreateSymbolicLinkTransactedW>(original( (PatchFunction)PatchCreateSymbolicLinkTransactedW ));
            BOOL created = function( lpSymlinkFileName, lpTargetFileName, dwFlags, hTransaction );
            if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkTransactedW", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
            fileAccess( lpSymlinkFileName, AccessWrite );
            fileAccess( lpTargetFileName, AccessRead );
            return created;
        }
        typedef BOOL(*TypeCloseHandle)(HANDLE);
        BOOL PatchCloseHandle( HANDLE handle ) {
            auto function = reinterpret_cast<TypeCloseHandle>(original( (PatchFunction)PatchCloseHandle ));
            // Record last write time when closing file opened for write
            wstring fileName = fileAccess( handle );
            BOOL closed = function( handle );
            if  (fileName != L"") if (debugLog( PatchExecution )) debugMessage( "CloseHandle", closed ) << handleCode( handle ) << L" ) on " << fileName.c_str() << record;
            return closed;
        }

        // Convert Windows access mode value to FileAccessMode value
        FileAccessMode requestedAccessMode( DWORD desiredAccess ) {
            FileAccessMode mode = 0;
            if (desiredAccess & GENERIC_ALL) mode |= (AccessRead | AccessWrite);
            if (desiredAccess & GENERIC_READ) mode |= AccessRead;
            if (desiredAccess & GENERIC_WRITE) mode |= AccessWrite;
            if (desiredAccess & DELETE) mode |= AccessDelete;
            return mode;
        }
        // Convert old stlye Windows access mode value to FileAccessMode value
        FileAccessMode requestedAccessMode( UINT desiredAccess ) {
            FileAccessMode mode = 0;
            if (desiredAccess & OF_CREATE) mode |= AccessWrite;
            if (desiredAccess & OF_DELETE) mode |= AccessDelete;
            if (desiredAccess & OF_EXIST) mode |= AccessRead;
            if (desiredAccess & OF_PARSE) mode |= AccessRead;
            if (desiredAccess & OF_PROMPT) mode |= AccessRead; // Not sure about intention
            if (desiredAccess & OF_READ) mode |= AccessRead;
            if (desiredAccess & OF_READWRITE) mode |= (AccessRead | AccessWrite);
            if (desiredAccess & OF_REOPEN) mode |= (AccessRead | AccessWrite); // Not sure about intention
            if (desiredAccess & OF_VERIFY) mode |= AccessRead;
            if (desiredAccess & OF_WRITE) mode |= AccessWrite;
            return mode;
        }

        inline wstring widen( const string& fileName ) {
        if (fileName.empty()) return {};
        int len = MultiByteToWideChar( CP_UTF8, 0, &fileName[0], fileName.size(), NULL, 0 );
        wstring wideFileName( len, 0 );
        MultiByteToWideChar( CP_UTF8, 0, &fileName[0], fileName.size(), &wideFileName[0], len );
        return wideFileName;
        }

        // Extract file name from handle to opened file
        // Will return empty string if handle does not refer to a file
        wstring fullName( HANDLE handle ) {
            wchar_t fileNameString[ MaxFileName ];
            wstring fileName;
            if ( GetFinalPathNameByHandleW( handle, fileNameString, MaxFileName, 0 ) ) {
                fileName = fileNameString;
                if (fileName.starts_with( L"\\\\?\\" ))  fileName = &fileNameString[ 4 ];
                if (fileName.starts_with( L"\\\\.\\" ))  fileName = &fileNameString[ 4 ];
            }
            return path( fileName ).generic_wstring();
        }
        // Expand file name to full path (according to Windows semantics)
        // Returns empty string if file name expansion fails.
        wstring fullName( const wchar_t* fileName ) {
            wchar_t filePath[ MaxFileName ];
            wchar_t* fileNameAddress;
            DWORD length = GetFullPathNameW( fileName, MaxFileName, filePath, &fileNameAddress );
            if (length == 0) return wstring( L"" );
            return path( filePath ).generic_wstring();
        }
        wstring fullName( const char* fileName ) {
            return fullName( widen( string( fileName ) ).c_str() );
        }
        wstring fullName( const wstring& fileName ) {
            return fullName( fileName.c_str() );
        }

        void recordFileEvent( const wstring& file, FileAccessMode mode, const FileTime& time ) {
            if (recordingEvents()) eventRecord() << file << L" [ " << time << L" ] " << modeToString( mode ) << record;
        }

        // Retrieve access rights from opened handle...
        // FileAccessMode getAccessRights( HANDLE handle ) {
        //     FileAccessMode mode = AccessNone;
        //     PUBLIC_OBJECT_BASIC_INFORMATION info;
        //     auto status = NtQueryObject( handle, ObjectBasicInformation, &info, sizeof( info ), nullptr );
        //     if (status == STATUS_SUCCESS) {
        //         auto rights = info.GrantedAccess;
        //         if (rights & DELETE) mode |= AccessDelete;
        //         if (rights & GENERIC_READ) mode |= AccessRead;
        //         if (rights & READ_CONTROL) mode |= AccessRead;
        //         if (rights & GENERIC_WRITE) mode |= AccessWrite;
        //         if (rights & WRITE_OWNER) mode |= AccessWrite;
        //         if (rights & WRITE_DAC) mode |= AccessWrite;
        //         if (rights & DELETE) mode |= AccessDelete;
        //         if (rights & GENERIC_ALL) mode |= (AccessRead | AccessWrite | AccessDelete);
        //     }
        //     return mode;
        // }

        FileTime getLastWriteTime( const wstring& fileName ) {
            WIN32_FILE_ATTRIBUTE_DATA attributes;
            auto getAttributes = reinterpret_cast<TypeGetFileAttributesExW>(original( (PatchFunction)PatchGetFileAttributesExW ));
            if (getAttributes( fileName.c_str(), GetFileExInfoStandard, &attributes ) ) {
                int64_t timeValue = (static_cast<int64_t>(attributes.ftLastWriteTime.dwHighDateTime) << 32)  + attributes.ftLastWriteTime.dwLowDateTime;
                // timeValue /= 10; // ToDo: Avoid conversion to us ticks, define duration with 100 ns ticks
                auto fileTime = FileTime( chrono::duration<int64_t,ratio<1,10000000>>( timeValue ) );
                if (debugLog( WriteTime )) debugMessage( "getLastWriteTime" ) << fileName << " ) = [ " << fileTime << " ]" << record;
                return fileTime;
            }
            if (debugLog( WriteTime )) debugMessage( "getLastWriteTime" ) << fileName << " ) failed with error : " << GetLastErrorString() << record;
            return FileTime();
        }
        // Register file access mode on file (path)
        wstring fileAccess( const wstring& fileName, FileAccessMode mode ) {
            DWORD error = GetLastError();
            auto fullFileName = fullName( fileName.c_str() );
            if (fullFileName != L"") {
                if (debugLog( FileAccesses )) debugRecord() << L"MonitorFiles - " << modeToString( mode ) << L" access by name on file " << fullFileName << record;
                recordFileEvent( fullFileName, mode, getLastWriteTime( fileName ) );
            }
            SetLastError( error );
            return fullFileName;
        }
        wstring fileAccess( const string& fileName, FileAccessMode mode ) {
            return fileAccess( widen( fileName ), mode );
        }
        wstring fileAccess( HANDLE handle, FileAccessMode mode ) {
            DWORD error = GetLastError();
            auto fullFileName = fullName( handle );
            if (fullFileName != L"") {
                // Record last write time on file being closed
//                if ((getAccessRights( handle ) & AccessDelete) == 0) {
                    // Record last write time on file being closed but not being deleted...
                    if (debugLog( FileAccesses )) debugRecord() << L"MonitorFiles - " << modeToString( mode ) << L" access by handle " << handleCode( handle) << " on file " << fullFileName << record;
                    recordFileEvent( fullFileName, mode, getLastWriteTime( fullFileName ) );
//                }
            } else if (mode == AccessNone) {
                // Possibly closing handle on a thread
                DWORD id = GetThreadId( handle );
                if (id != 0) {
                    auto thread = GetThreadID( id );
                    if (debugLog( FileAccesses )) debugRecord() << L"MonitorFiles - Thread " << thread << " terminated" << record;
                    RemoveSessionThread( thread );
                }
            }
            SetLastError( error );
            return fullFileName;
        }

    }

    void registerFileAccess() {
        registerPatch( "CreateDirectoryA", (PatchFunction)PatchCreateDirectoryA );
        registerPatch( "CreateDirectoryW", (PatchFunction)PatchCreateDirectoryW );
        registerPatch( "RemoveDirectoryA", (PatchFunction)PatchRemoveDirectoryA );
        registerPatch( "RemoveDirectoryW", (PatchFunction)PatchRemoveDirectoryW );
        registerPatch( "CreateFileA", (PatchFunction)PatchCreateFileA );
        registerPatch( "CreateFileW", (PatchFunction)PatchCreateFileW );
        registerPatch( "TypeCreateFileTransactedA", (PatchFunction)PatchCreateFileTransactedA );
        registerPatch( "TypeCreateFileTransactedW", (PatchFunction)PatchCreateFileTransactedW );
        registerPatch( "CreateFile2", (PatchFunction)PatchCreateFile2 );
        registerPatch( "ReOpenFile", (PatchFunction)PatchReOpenFile );
        registerPatch( "ReplaceFileA", (PatchFunction)PatchReplaceFileA );
        registerPatch( "ReplaceFileW", (PatchFunction)PatchReplaceFileW );
        registerPatch( "OpenFile", (PatchFunction)PatchOpenFile );
        registerPatch( "CreateHardLinkA", (PatchFunction)PatchCreateHardLinkA );
        registerPatch( "CreateHardLinkW", (PatchFunction)PatchCreateHardLinkW );
        registerPatch( "CreateHardLinkTransactedA", (PatchFunction)PatchCreateHardLinkTransactedA );
        registerPatch( "CreateHardLinkTransactedW", (PatchFunction)PatchCreateHardLinkTransactedW );
        registerPatch( "CreateSymbolicLinkA", (PatchFunction)PatchCreateSymbolicLinkA );
        registerPatch( "CreateSymbolicLinkW", (PatchFunction)PatchCreateSymbolicLinkW );
        registerPatch( "CreateSymbolicLinkTransactedA", (PatchFunction)PatchCreateSymbolicLinkTransactedA );
        registerPatch( "CreateSymbolicLinkTransactedW", (PatchFunction)PatchCreateSymbolicLinkTransactedW );
        registerPatch( "DeleteFileA", (PatchFunction)PatchDeleteFileA );
        registerPatch( "DeleteFileW", (PatchFunction)PatchDeleteFileW );
        registerPatch( "DeleteFileTransactedA", (PatchFunction)PatchDeleteFileTransactedA );
        registerPatch( "DeleteFileTransactedW", (PatchFunction)PatchDeleteFileTransactedW );
        registerPatch( "CopyFileA", (PatchFunction)PatchCopyFileA );
        registerPatch( "CopyFileW", (PatchFunction)PatchCopyFileW );
        registerPatch( "CopyFileExA", (PatchFunction)PatchCopyFileExA );
        registerPatch( "CopyFileExW", (PatchFunction)PatchCopyFileExW );
        registerPatch( "CopyFileTransactedA", (PatchFunction)PatchCopyFileTransactedA );
        registerPatch( "CopyFileTransactedW", (PatchFunction)PatchCopyFileTransactedW );
        registerPatch( "CopyFile2", (PatchFunction)PatchCopyFile2 );
        registerPatch( "MoveFileA", (PatchFunction)PatchMoveFileA );
        registerPatch( "MoveFileW", (PatchFunction)PatchMoveFileW );
        registerPatch( "MoveFileExA", (PatchFunction)PatchMoveFileExA );
        registerPatch( "MoveFileExW", (PatchFunction)PatchMoveFileExW );
        registerPatch( "MoveFileWithProgressA", (PatchFunction)PatchMoveFileWithProgressA );
        registerPatch( "MoveFileWithProgressW", (PatchFunction)PatchMoveFileWithProgressW );
        registerPatch( "MoveFileWithProgressTransactedA", (PatchFunction)PatchMoveFileWithProgressTransactedA );
        registerPatch( "MoveFileWithProgressTransactedW", (PatchFunction)PatchMoveFileWithProgressTransactedW );
        registerPatch( "FindFirstFileA", (PatchFunction)PatchFindFirstFileA );
        registerPatch( "FindFirstFileW", (PatchFunction)PatchFindFirstFileW );
        registerPatch( "FindFirstFileExA", (PatchFunction)PatchFindFirstFileExA );
        registerPatch( "FindFirstFileExW", (PatchFunction)PatchFindFirstFileExW );
        registerPatch( "FindFirstFileTransactedA", (PatchFunction)PatchFindFirstFileTransactedA );
        registerPatch( "FindFirstFileTransactedW", (PatchFunction)PatchFindFirstFileTransactedW );
        registerPatch( "GetFileAttributesA", (PatchFunction)PatchGetFileAttributesA );
        registerPatch( "GetFileAttributesW", (PatchFunction)PatchGetFileAttributesW );
        registerPatch( "GetFileAttributesExA", (PatchFunction)PatchGetFileAttributesExA );
        registerPatch( "GetFileAttributesExW", (PatchFunction)PatchGetFileAttributesExW );
        registerPatch( "GetFileAttributesTransactedA", (PatchFunction)PatchGetFileAttributesTransactedA );
        registerPatch( "GetFileAttributesTransactedW", (PatchFunction)PatchGetFileAttributesTransactedW );
        registerPatch( "SetFileAttributesA", (PatchFunction)PatchSetFileAttributesA );
        registerPatch( "SetFileAttributesW", (PatchFunction)PatchSetFileAttributesW );
        registerPatch( "CloseHandle", (PatchFunction)PatchCloseHandle );
    }
    void unregisterFileAccess() {
        unregisterPatch( "CreateDirectoryA" );
        unregisterPatch( "CreateDirectoryW" );
        unregisterPatch( "RemoveDirectoryA" );
        unregisterPatch( "RemoveDirectoryW" );
        unregisterPatch( "CreateFileA" );
        unregisterPatch( "CreateFileW" );
        unregisterPatch( "TypeCreateFileTransactedA" );
        unregisterPatch( "TypeCreateFileTransactedW" );
        unregisterPatch( "CreateFile2" );
        unregisterPatch( "ReOpenFile" );
        unregisterPatch( "ReplaceFileA" );
        unregisterPatch( "ReplaceFileW" );
        unregisterPatch( "OpenFile" );
        unregisterPatch( "CreateHardLinkA" );
        unregisterPatch( "CreateHardLinkW" );
        unregisterPatch( "CreateHardLinkTransactedA" );
        unregisterPatch( "CreateHardLinkTransactedW" );
        unregisterPatch( "CreateSymbolicLinkA" );
        unregisterPatch( "CreateSymbolicLinkW" );
        unregisterPatch( "CreateSymbolicLinkTransactedA" );
        unregisterPatch( "CreateSymbolicLinkTransactedW" );
        unregisterPatch( "DeleteFileA" );
        unregisterPatch( "DeleteFileW" );
        unregisterPatch( "DeleteFileTransactedA" );
        unregisterPatch( "DeleteFileTransactedW" );
        unregisterPatch( "CopyFileA" );
        unregisterPatch( "CopyFileW" );
        unregisterPatch( "CopyFileExA" );
        unregisterPatch( "CopyFileExW" );
        unregisterPatch( "CopyFileTransactedA" );
        unregisterPatch( "CopyFileTransactedW" );
        unregisterPatch( "CopyFile2" );
        unregisterPatch( "MoveFileA" );
        unregisterPatch( "MoveFileW" );
        unregisterPatch( "MoveFileExA" );
        unregisterPatch( "MoveFileExW" );
        unregisterPatch( "MoveFileWithProgressA" );
        unregisterPatch( "MoveFileWithProgressW" );
        unregisterPatch( "MoveFileWithProgressTransactedA" );
        unregisterPatch( "MoveFileWithProgressTransactedW" );
        unregisterPatch( "FindFirstFileA" );
        unregisterPatch( "FindFirstFileW" );
        unregisterPatch( "FindFirstFileExA" );
        unregisterPatch( "FindFirstFileExW" );
        unregisterPatch( "FindFirstFileTransactedA" );
        unregisterPatch( "FindFirstFileTransactedW" );
        unregisterPatch( "GetFileAttributesA" );
        unregisterPatch( "GetFileAttributesW" );
        unregisterPatch( "GetFileAttributesExA" );
        unregisterPatch( "GetFileAttributesExW" );
        unregisterPatch( "GetFileAttributesTransactedA" );
        unregisterPatch( "GetFileAttributesTransactedW" );
        unregisterPatch( "SetFileAttributesA" );
        unregisterPatch( "SetFileAttributesW" );
        unregisterPatch( "CloseHandle" );
    }

} // namespace AccessMonitor