#include "Monitor.h"
#include "MonitorFiles.h"
#include "FileAccess.h"
#include "MonitorLogging.h"
#include "Patch.h"
#include "Session.h"

#include <windows.h>
#include <winternl.h>
#include <filesystem>

using namespace std;
using namespace std::filesystem;

// Windows Overlay Filter (Wof) functions are not implemented.
// Windows on Windows (WoW) functions are not implemented.
// Lempel-Ziv (LZ) functions are not implemented.

// ToDo: Add function calling convention to all externals
// ToDo: Implenent for Linux and MacOS, current implementation is Windows only...

// ToDo: Drop-out of patch function quickly when not in a session.

namespace AccessMonitor {

    namespace {

        class FileMonitorGuard : public MonitorGuard {
        public:
            FileMonitorGuard() : MonitorGuard( SessionFileAccess() ) {}
        };

        FileAccessMode requestedAccessMode( DWORD desiredAccess );
        FileAccessMode requestedAccessMode( UINT desiredAccess );

        wstring fileAccess( const string& fileName, FileAccessMode mode );
        wstring fileAccess( const wstring& fileName, FileAccessMode mode );
        wstring fileAccess( HANDLE handle, FileAccessMode mode = AccessNone );

        inline uint64_t handleCode( HANDLE handle ) { return reinterpret_cast<uint64_t>( handle ); }

        inline wostream& debugMessage( const char* function ) {
            return debugRecord() << L"MonitorFiles - " << function << "( ";
        }
        inline wostream& debugMessage( const char* function, bool success ) {
            DWORD errorCode = GetLastError();
            if (!success && (errorCode != ERROR_SUCCESS)) debugRecord() << L"MonitorFiles - " << function << L" failed with error : " << lasErrorString( errorCode ) << record;
            return debugMessage( function );
        }
        inline wostream& debugMessage( const char* function, HANDLE handle ) {
            return debugMessage( function, (handle != INVALID_HANDLE_VALUE) );
        }

        BOOL PatchCreateDirectoryA(
            LPCSTR                pathName,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto created = patchOriginal( PatchCreateDirectoryA )( pathName, securityAttributes );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryA", created ) << pathName << L", ... )" << record;
                fileAccess( pathName, AccessWrite );
            }
            return created;
        }
        BOOL PatchCreateDirectoryW(
            LPCWSTR                pathName,
            LPSECURITY_ATTRIBUTES  securityAttributes
        ) {
            auto created = patchOriginal( PatchCreateDirectoryW )( pathName, securityAttributes );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryW", created ) << pathName << L", ... )" << record;
                fileAccess( pathName, AccessWrite );
            }
            return created;
        }
        BOOL PatchCreateDirectoryExA(
            LPCSTR                templateDirectory,
            LPCSTR                newDirectory,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto created = patchOriginal( PatchCreateDirectoryExA )( templateDirectory, newDirectory, securityAttributes );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryExA", created ) << newDirectory << L", ... )" << record;
                fileAccess( newDirectory, AccessWrite );
            }
            return created;
        }
        BOOL PatchCreateDirectoryExW(
            LPCWSTR                templateDirectory,
            LPCWSTR                newDirectory,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto created = patchOriginal( PatchCreateDirectoryExW )( templateDirectory, newDirectory, securityAttributes );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryExA", created ) << newDirectory << L", ... )" << record;
                fileAccess( newDirectory, AccessWrite );
            }
            return created;
        }
        BOOL PatchRemoveDirectoryA(
            LPCSTR pathName
        ) {
            auto removed = patchOriginal( PatchRemoveDirectoryA )( pathName );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "RemoveDirectoryA", removed ) << pathName << L", ... )" << record;
                fileAccess( pathName, AccessDelete );
            }
            return removed;
        }
        BOOL PatchRemoveDirectoryW(
            LPCWSTR pathName
        ) {
            auto removed = patchOriginal( PatchRemoveDirectoryW )( pathName );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "RemoveDirectoryW", removed ) << pathName << L", ... )" << record;
                fileAccess( pathName, AccessDelete );
            }
            return removed;
        }
        HANDLE PatchCreateFileA(
            LPCSTR                fileName,
            DWORD                 dwDesiredAccess,
            DWORD                 dwShareMode,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            DWORD                 dwCreationDisposition,
            DWORD                 dwFlagsAndAttributes,
            HANDLE                hTemplateFile
        ) {
            auto handle = patchOriginal( PatchCreateFileA )( fileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateFileA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            }
            return handle;
        }

        HANDLE PatchCreateFileW(
            LPCWSTR               fileName,
            DWORD                 dwDesiredAccess,
            DWORD                 dwShareMode,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            DWORD                 dwCreationDisposition,
            DWORD                 dwFlagsAndAttributes,
            HANDLE                hTemplateFile
        ) {
            auto handle = patchOriginal( PatchCreateFileW )( fileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateFileW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            }
            return handle;
        }
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
            auto handle = patchOriginal( PatchCreateFileTransactedA )( fileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, hTransaction, pusMiniVersion, lpExtendedParameter );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateFileTransactedA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );

            }
            return handle;
        }
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
            auto handle = patchOriginal( PatchCreateFileTransactedW )( fileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, hTransaction, pusMiniVersion, lpExtendedParameter );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateFileTransactedW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            }
            return handle;
        }
        HANDLE PatchCreateFile2(
            LPCWSTR                           fileName,
            DWORD                             dwDesiredAccess,
            DWORD                             dwShareMode,
            DWORD                             dwCreationDisposition,
            LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams
        ) {
            auto handle = patchOriginal( PatchCreateFile2 )( fileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateFile2", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            }
            return handle;
        }
        HANDLE PatchReOpenFile(
            HANDLE hOriginalFile,
            DWORD  dwDesiredAccess,
            DWORD  dwShareMode,
            DWORD  dwFlagsAndAttributes
        ) {
            auto handle = patchOriginal( PatchReOpenFile )( hOriginalFile, dwDesiredAccess, dwShareMode, dwFlagsAndAttributes );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                auto fileName = fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
                if (debugLog( PatchExecution )) debugMessage( "ReOpenFile", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
            }
            return handle;
        }
        HFILE PatchOpenFile(
            LPCSTR     fileName,
            LPOFSTRUCT lpReOpenBuff,
            UINT       uStyle
        ) {
            auto handle = patchOriginal( PatchOpenFile )( fileName, lpReOpenBuff, uStyle );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "OpenFile", (handle != HFILE_ERROR) ) << fileName << L", ... ) -> " << handle << record;
                fileAccess( fileName, requestedAccessMode( uStyle ) );
            }
            return handle;

        }
        BOOL PatchDeleteFileA( LPCSTR fileName ) {
            auto deleted = patchOriginal( PatchDeleteFileA )( fileName );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "DeleteFileA", deleted ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessDelete );
            }
            return deleted;
        }
        BOOL PatchDeleteFileW( LPCWSTR fileName ) {
            auto deleted = patchOriginal( PatchDeleteFileW )( fileName );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "DeleteFileW", deleted ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessDelete );
            }
            return deleted;
        }
        BOOL PatchDeleteFileTransactedA( LPCSTR fileName, HANDLE hTransaction ) {
            auto deleted = patchOriginal( PatchDeleteFileTransactedA )( fileName, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "DeleteFileTransactedA", deleted ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessDelete );
            }
            return deleted;
        }
        BOOL PatchDeleteFileTransactedW( LPCWSTR fileName, HANDLE hTransaction ) {
            auto deleted = patchOriginal( PatchDeleteFileTransactedW )( fileName, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "DeleteFileTransactedW", deleted ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessDelete );
            }
            return deleted;
        }
        BOOL PatchCopyFileA(
            LPCSTR existingFileName,
            LPCSTR newFileName,
            BOOL   failIfExists
        ) {
            auto copied = patchOriginal( PatchCopyFileA )( existingFileName, newFileName, failIfExists );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileA", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead );
                fileAccess( newFileName, AccessWrite );
            }
            return copied;
        }
        BOOL PatchCopyFileW(
            LPCWSTR existingFileName,
            LPCWSTR newFileName,
            BOOL    failIfExists
        ) {
            auto copied = patchOriginal( PatchCopyFileW )( existingFileName, newFileName, failIfExists );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileW", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead );
                fileAccess( newFileName, AccessWrite );
            }
            return copied;
        }
        BOOL PatchCopyFileExA(
            LPCSTR             existingFileName,
            LPCSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            LPBOOL             pbCancel,
            DWORD              dwCopyFlags
        ) {
            auto copied = patchOriginal( PatchCopyFileExA )( existingFileName, newFileName, lpProgressRoutine, lpData, pbCancel, dwCopyFlags );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileExA", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead );
                fileAccess( newFileName, AccessWrite );
            }
            return copied;
        }
        BOOL PatchCopyFileExW(
            LPCWSTR             existingFileName,
            LPCWSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            LPBOOL             pbCancel,
            DWORD              dwCopyFlags
        ) {
            auto copied = patchOriginal( PatchCopyFileExW )( existingFileName, newFileName, lpProgressRoutine, lpData, pbCancel, dwCopyFlags );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileExW", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead );
                fileAccess( newFileName, AccessWrite );
            }
            return copied;
        }
        BOOL PatchCopyFileTransactedA(
            LPCSTR             existingFileName,
            LPCSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            LPBOOL             pbCancel,
            DWORD              dwCopyFlags,
            HANDLE             hTransaction
        ) {
            auto copied = patchOriginal( PatchCopyFileTransactedA )( existingFileName, newFileName, lpProgressRoutine,lpData, pbCancel, dwCopyFlags, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileTransactedA", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead );
                fileAccess( newFileName, AccessWrite );
            }
            return copied;
        }
        BOOL PatchCopyFileTransactedW(
            LPCWSTR            existingFileName,
            LPCWSTR            newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            LPBOOL             pbCancel,
            DWORD              dwCopyFlags,
            HANDLE             hTransaction
        ) {
            auto copied = patchOriginal( PatchCopyFileTransactedW )( existingFileName, newFileName, lpProgressRoutine,lpData, pbCancel, dwCopyFlags, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileTransactedW", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead );
                fileAccess( newFileName, AccessWrite );
            }
            return copied;
        }
        HRESULT PatchCopyFile2(
            PCWSTR                         existingFileName,
            PCWSTR                         newFileName,
            COPYFILE2_EXTENDED_PARAMETERS* pExtendedParameters
        ) {
            auto result = patchOriginal( PatchCopyFile2 )( existingFileName, newFileName, pExtendedParameters );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFile2", result== S_OK) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead );
                fileAccess( newFileName, AccessWrite );
            }
            return result;
        }
        BOOL PatchReplaceFileA(
            LPCSTR  lpReplacedFileName,
            LPCSTR  lpReplacementFileName,
            LPCSTR  lpBackupFileName,
            DWORD   dwReplaceFlags,
            LPVOID  lpExclude,
            LPVOID  lpReserved
        ) {
            auto replaced = patchOriginal( PatchReplaceFileA )( lpReplacedFileName, lpReplacementFileName, lpBackupFileName, dwReplaceFlags, lpExclude, lpReserved );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "ReplaceFileA", replaced ) << lpReplacedFileName << L", " << lpReplacementFileName << L", ... )" << record;
                fileAccess( lpReplacementFileName, AccessRead );
                fileAccess( lpReplacedFileName, AccessWrite );
                if (lpBackupFileName!= nullptr) fileAccess( lpBackupFileName, AccessWrite );
            }
            return replaced;
        }
        BOOL PatchReplaceFileW(
            LPCWSTR lpReplacedFileName,
            LPCWSTR lpReplacementFileName,
            LPCWSTR lpBackupFileName,
            DWORD   dwReplaceFlags,
            LPVOID  lpExclude,
            LPVOID  lpReserved
        ) {
            auto replaced = patchOriginal( PatchReplaceFileW )( lpReplacedFileName, lpReplacementFileName, lpBackupFileName, dwReplaceFlags, lpExclude, lpReserved );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "ReplaceFileW", replaced ) << lpReplacedFileName << L", " << lpReplacementFileName << L", ... )" << record;
                fileAccess( lpReplacementFileName, AccessRead );
                fileAccess( lpReplacedFileName, AccessWrite );
                if (lpBackupFileName!= nullptr) fileAccess( lpBackupFileName, AccessWrite );
            }
            return replaced;
        }
        BOOL PatchMoveFileA(
            LPCSTR existingFileName,
            LPCSTR newFileName
        ) {
            auto moved = patchOriginal( PatchMoveFileA )( existingFileName, newFileName );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete );
                fileAccess( newFileName, AccessWrite );
            }
            return moved;
        }       
        BOOL PatchMoveFileW(
            LPCWSTR existingFileName,
            LPCWSTR newFileName
        ) {
            auto moved = patchOriginal( PatchMoveFileW )( existingFileName, newFileName );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete );
                fileAccess( newFileName, AccessWrite );
            }
            return moved;
        }
        BOOL PatchMoveFileExA(
            LPCSTR  existingFileName,
            LPCSTR  newFileName,
            DWORD   flags
        ) {
            auto moved = patchOriginal( PatchMoveFileExA )( existingFileName, newFileName, flags );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileExA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete );
                fileAccess( newFileName, AccessWrite );
            }
            return moved;
        }       
        BOOL PatchMoveFileExW(
            LPCWSTR existingFileName,
            LPCWSTR newFileName,
            DWORD   flags
        ) {
            auto moved = patchOriginal( PatchMoveFileExW )( existingFileName, newFileName, flags );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileExW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete );
                fileAccess( newFileName, AccessWrite );
            }
            return moved;
        }       
        BOOL PatchMoveFileWithProgressA(
            LPCSTR             existingFileName,
            LPCSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            DWORD              dwFlags
        ) {
            auto moved = patchOriginal( PatchMoveFileWithProgressA )( existingFileName, newFileName, lpProgressRoutine, lpData, dwFlags );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileWithProgressA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete );
                fileAccess( newFileName, AccessWrite );
            }
            return moved;
        }
        BOOL PatchMoveFileWithProgressW(
            LPCWSTR             existingFileName,
            LPCWSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            DWORD              dwFlags
        ) {
            auto moved = patchOriginal( PatchMoveFileWithProgressW )( existingFileName, newFileName, lpProgressRoutine, lpData, dwFlags );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileWithProgressW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete );
                fileAccess( newFileName, AccessWrite );
            }
            return moved;
        }
        BOOL PatchMoveFileWithProgressTransactedA(
            LPCSTR             existingFileName,
            LPCSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            DWORD              dwFlags,
            HANDLE             hTransaction
        ) {
            auto moved = patchOriginal( PatchMoveFileWithProgressTransactedA )( existingFileName, newFileName, lpProgressRoutine, lpData, dwFlags, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "PatchMoveFileWithProgressTransactedA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete );
                fileAccess( newFileName, AccessWrite );
            }
            return moved;
        }
        BOOL PatchMoveFileWithProgressTransactedW(
            LPCWSTR             existingFileName,
            LPCWSTR             newFileName,
            LPPROGRESS_ROUTINE lpProgressRoutine,
            LPVOID             lpData,
            DWORD              dwFlags,
            HANDLE             hTransaction
        ) {
            auto moved = patchOriginal( PatchMoveFileWithProgressTransactedW )( existingFileName, newFileName, lpProgressRoutine, lpData, dwFlags, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "PatchMoveFileWithProgressTransactedW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete );
                fileAccess( newFileName, AccessWrite );
            }
            return moved;
        }
        HANDLE PatchFindFirstFileA(
            LPCSTR             fileName,
            LPWIN32_FIND_DATAA lpFindFileData
        ) {
            auto handle = patchOriginal( PatchFindFirstFileA )( fileName, lpFindFileData );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead );
            }
            return handle;
        }        
        HANDLE PatchFindFirstFileW(
            LPCWSTR            fileName,
            LPWIN32_FIND_DATAW lpFindFileData
        ) {
            auto handle = patchOriginal( PatchFindFirstFileW )( fileName, lpFindFileData );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead );
            }
            return handle;
        }
        HANDLE PatchFindFirstFileExA(
            LPCSTR             fileName,
            FINDEX_INFO_LEVELS fInfoLevelId,
            LPVOID             lpFindFileData,
            FINDEX_SEARCH_OPS  fSearchOp,
            LPVOID             lpSearchFilter,
            DWORD              dwAdditionalFlags
        ) {
            auto handle = patchOriginal( PatchFindFirstFileExA )( fileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileExA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead );
            }
            return handle;
        }
        HANDLE PatchFindFirstFileExW(
            LPCWSTR            fileName,
            FINDEX_INFO_LEVELS fInfoLevelId,
            LPVOID             lpFindFileData,
            FINDEX_SEARCH_OPS  fSearchOp,
            LPVOID             lpSearchFilter,
            DWORD              dwAdditionalFlags
        ) {
            auto handle = patchOriginal( PatchFindFirstFileExW )( fileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileExW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead );
            }
            return handle;
        }
        HANDLE PatchFindFirstFileTransactedA(
            LPCSTR             fileName,
            FINDEX_INFO_LEVELS fInfoLevelId,
            LPVOID             lpFindFileData,
            FINDEX_SEARCH_OPS  fSearchOp,
            LPVOID             lpSearchFilter,
            DWORD              dwAdditionalFlags,
            HANDLE             hTransaction
        ) {
            auto handle = patchOriginal( PatchFindFirstFileTransactedA )( fileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileTransactedA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead );
            }
            return handle;
        }
        HANDLE PatchFindFirstFileTransactedW(
            LPCWSTR            fileName,
            FINDEX_INFO_LEVELS fInfoLevelId,
            LPVOID             lpFindFileData,
            FINDEX_SEARCH_OPS  fSearchOp,
            LPVOID             lpSearchFilter,
            DWORD              dwAdditionalFlags,
            HANDLE             hTransaction
        ) {
            auto handle = patchOriginal( PatchFindFirstFileTransactedW )( fileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileTransactedW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead );
            }
            return handle;
        }
        DWORD PatchGetFileAttributesA(
            LPCSTR fileName
        ) {
            auto attributes = patchOriginal( PatchGetFileAttributesA )( fileName );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesA", (attributes != INVALID_FILE_ATTRIBUTES) ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead );
            }
            return attributes;
        }
        DWORD PatchGetFileAttributesW(
            LPCWSTR fileName
        ) {
            auto attributes = patchOriginal( PatchGetFileAttributesW )( fileName );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesW", (attributes != INVALID_FILE_ATTRIBUTES) ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead );
            }
            return attributes;
        }
        BOOL PatchGetFileAttributesExA(
            LPCSTR                 fileName,
            GET_FILEEX_INFO_LEVELS fInfoLevelId,
            LPVOID                 lpFileInformation
        ) {
            auto got = patchOriginal( PatchGetFileAttributesExA )( fileName, fInfoLevelId, lpFileInformation );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesExA", got ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead );
            }
            return got;
        }
        BOOL PatchGetFileAttributesExW(
            LPCWSTR                 fileName,
            GET_FILEEX_INFO_LEVELS fInfoLevelId,
            LPVOID                 lpFileInformation

        ) {
            auto got = patchOriginal( PatchGetFileAttributesExW )( fileName, fInfoLevelId, lpFileInformation );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesExW", got ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead );
            }
            return got;
        }
        BOOL PatchGetFileAttributesTransactedA(
            LPCSTR                 fileName,
            GET_FILEEX_INFO_LEVELS fInfoLevelId,
            LPVOID                 lpFileInformation,
            HANDLE                 hTransaction
        ) {
            bool got = patchOriginal( PatchGetFileAttributesTransactedA )( fileName, fInfoLevelId, lpFileInformation, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesTransactedA", got ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead );
            }
            return got;
        }
        BOOL PatchGetFileAttributesTransactedW(
            LPCWSTR                 fileName,
            GET_FILEEX_INFO_LEVELS fInfoLevelId,
            LPVOID                 lpFileInformation,
            HANDLE                 hTransaction
        ) {
            auto got = patchOriginal( PatchGetFileAttributesTransactedW )( fileName, fInfoLevelId, lpFileInformation, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesTransactedW", got ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead );
            }
            return got;
        }
        BOOL PatchSetFileAttributesA(
            LPCSTR fileName,
            DWORD  dwFileAttributes
        ) {
            auto set = patchOriginal( PatchSetFileAttributesA )( fileName, dwFileAttributes );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "PatchSetFileAttributesA", set ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessWrite );
            }
            return set;
        }
        BOOL PatchSetFileAttributesW(
            LPCWSTR fileName,
            DWORD  dwFileAttributes
        ) {
            auto set = patchOriginal( PatchSetFileAttributesW )( fileName, dwFileAttributes );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "PatchSetFileAttributesW", set ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessWrite );
            }
            return set;
        }
        BOOL PatchCreateHardLinkA(
            LPCSTR                lpFileName,
            LPCSTR                lpExistingFileName,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes
        ) {
            auto created = patchOriginal( PatchCreateHardLinkA )( lpFileName, lpExistingFileName, lpSecurityAttributes );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkA", created )  << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
                fileAccess( lpFileName, AccessWrite );
                fileAccess( lpExistingFileName, AccessRead );
            }
            return created;
        }
        BOOL PatchCreateHardLinkW(
            LPCWSTR                lpFileName,
            LPCWSTR                lpExistingFileName,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes
        ) {
            auto created = patchOriginal( PatchCreateHardLinkW )( lpFileName, lpExistingFileName, lpSecurityAttributes );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkW", created )  << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
                fileAccess( lpFileName, AccessWrite );
                fileAccess( lpExistingFileName, AccessRead );
            }
            return created;
        }
        BOOL PatchCreateHardLinkTransactedA(
            LPCSTR                lpFileName,
            LPCSTR                lpExistingFileName,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            HANDLE                hTransaction
        ) {
            auto created = patchOriginal( PatchCreateHardLinkTransactedA )( lpFileName, lpExistingFileName, lpSecurityAttributes, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkTransactedA", created ) << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
                fileAccess( lpFileName, AccessWrite );
                fileAccess( lpExistingFileName, AccessRead );
            }
            return created;
        }
        BOOL PatchCreateHardLinkTransactedW(
            LPCWSTR               lpFileName,
            LPCWSTR               lpExistingFileName,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            HANDLE                hTransaction
        ) {
            auto created = patchOriginal( PatchCreateHardLinkTransactedW )( lpFileName, lpExistingFileName, lpSecurityAttributes, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkTransactedW", created ) << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
                fileAccess( lpFileName, AccessWrite );
                fileAccess( lpExistingFileName, AccessRead );
            }
            return created;
        }
        BOOLEAN PatchCreateSymbolicLinkA(
            LPCSTR lpSymlinkFileName,
            LPCSTR lpTargetFileName,
            DWORD  dwFlags
        ) {
            auto created = patchOriginal( PatchCreateSymbolicLinkA )( lpSymlinkFileName, lpTargetFileName, dwFlags );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkA", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
                fileAccess( lpSymlinkFileName, AccessWrite );
                fileAccess( lpTargetFileName, AccessRead );
            }
            return created;
        }
        BOOLEAN PatchCreateSymbolicLinkW(
            LPCWSTR lpSymlinkFileName,
            LPCWSTR lpTargetFileName,
            DWORD   dwFlags
        ) {
            auto created = patchOriginal( PatchCreateSymbolicLinkW )( lpSymlinkFileName, lpTargetFileName, dwFlags );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkW", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
                fileAccess( lpSymlinkFileName, AccessWrite );
                fileAccess( lpTargetFileName, AccessRead );
            }
            return created;
        }
        BOOLEAN PatchCreateSymbolicLinkTransactedA(
            LPCSTR lpSymlinkFileName,
            LPCSTR lpTargetFileName,
            DWORD  dwFlags,
            HANDLE hTransaction
        ) {
            auto created = patchOriginal( PatchCreateSymbolicLinkTransactedA )( lpSymlinkFileName, lpTargetFileName, dwFlags, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkTransactedA", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
                fileAccess( lpSymlinkFileName, AccessWrite );
                fileAccess( lpTargetFileName, AccessRead );
            }
            return created;
        }
        BOOLEAN PatchCreateSymbolicLinkTransactedW(
            LPCWSTR lpSymlinkFileName,
            LPCWSTR lpTargetFileName,
            DWORD   dwFlags,
            HANDLE hTransaction
        ) {
            auto created = patchOriginal( PatchCreateSymbolicLinkTransactedW )( lpSymlinkFileName, lpTargetFileName, dwFlags, hTransaction );
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkTransactedW", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
                fileAccess( lpSymlinkFileName, AccessWrite );
                fileAccess( lpTargetFileName, AccessRead );
            }
            return created;
        }
        BOOL PatchCloseHandle( HANDLE handle ) {
            FileMonitorGuard guard;
            if (guard.monitoring() ) {
                // Record last write time when closing file opened for write (also for WithProgress calls)
                wstring fileName = fileAccess( handle, AccessNone );
                if  (fileName != L"") {
                    if (debugLog( PatchExecution )) debugMessage( "CloseHandle" ) << handleCode( handle ) << L" ) on " << fileName.c_str() << record;
                }
            }
            return patchOriginal( PatchCloseHandle )( handle );
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
            int len = MultiByteToWideChar( CP_UTF8, 0, &fileName[0], static_cast<int>(fileName.size()), NULL, 0 );
            wstring wideFileName( len, 0 );
            MultiByteToWideChar( CP_UTF8, 0, &fileName[0], static_cast<int>(fileName.size()), &wideFileName[0], len );
            return wideFileName;
        }

        wstring simplify(wchar_t const* fileName) {
            wstring simplePath = fileName;
            if ((simplePath.size() <= MAX_PATH) && simplePath.starts_with(L"\\\\?\\")) simplePath = &fileName[4];
            if (simplePath.starts_with(L"\\\\.\\")) simplePath = &fileName[4];
            std::erase(simplePath, '"');
            simplePath.insert(0, L"\"");
            simplePath.append(L"\"");
            return path(simplePath).generic_wstring();
        }

        // Extract file name from handle to opened file
        // Will return empty string if handle does not refer to a file
        wstring fullName( HANDLE handle ) {
            wchar_t fileNameString[ MaxFileName ];
            if ( GetFinalPathNameByHandleW( handle, fileNameString, MaxFileName, 0 ) ) {
                return simplify(fileNameString);
            }
            return path("").generic_wstring();
        }
        // Expand file name to full path (according to Windows semantics)
        // Returns empty string if file name expansion fails.
        wstring fullName( const wchar_t* fileName ) {
            wchar_t filePath[ MaxFileName ];
            wchar_t* fileNameAddress;
            wstring _fileName;
            DWORD length = GetFullPathNameW( fileName, MaxFileName, filePath, &fileNameAddress );
            return simplify(filePath);
        }
        wstring fullName( const char* fileName ) {
            return fullName( widen( string( fileName ) ).c_str() );
        }
        wstring fullName( const wstring& fileName ) {
            return fullName( fileName.c_str() );
        }

        void recordEvent( const wstring& file, FileAccessMode mode, const FileTime& time ) {
            if (recordingEvents()) eventRecord() << file << L" [" << time << L"] " << modeToString(mode) << record;
        }

        FileTime getLastWriteTime( const wstring& fileName ) {
            WIN32_FILE_ATTRIBUTE_DATA attributes;
            // Get attributes can succeed even when not referring to an actual file!
            if (GetFileAttributesExW( fileName.c_str(), GetFileExInfoStandard, &attributes ) && (attributes.dwFileAttributes != INVALID_FILE_ATTRIBUTES)) {
                int64_t timeValue = (static_cast<int64_t>(attributes.ftLastWriteTime.dwHighDateTime) << 32)  + attributes.ftLastWriteTime.dwLowDateTime;
                auto fileTime = FileTime( chrono::duration<int64_t,ratio<1,10000000>>( timeValue ) );
                if (debugLog( WriteTime )) debugMessage( "getLastWriteTime" ) << fileName << " ) = [ " << fileTime << " ]" << record;
                return fileTime;
            } else {
                DWORD errorCode = GetLastError();
                if (debugLog( WriteTime ) && (errorCode != ERROR_SUCCESS)) {
                    debugMessage( "GetFileAttributesExW" ) << fileName << " ) failed with error : " << lasErrorString( errorCode ) << record;
                }
            }
            return FileTime();
        }
        FileTime getLastWriteTime( HANDLE handle ) {
            BY_HANDLE_FILE_INFORMATION fileInformation;
            if (GetFileInformationByHandle( handle, &fileInformation) && (fileInformation.dwFileAttributes != INVALID_FILE_ATTRIBUTES)) {
                int64_t timeValue = (static_cast<int64_t>(fileInformation.ftLastWriteTime.dwHighDateTime) << 32)  + fileInformation.ftLastWriteTime.dwLowDateTime;
                auto fileTime = FileTime( chrono::duration<int64_t,ratio<1,10000000>>( timeValue ) );
                if (debugLog( WriteTime )) debugMessage( "getLastWriteTime" ) << handleCode( handle) << " ) = [ " << fileTime << " ]" << record;
                return fileTime;
            } else {
                DWORD errorCode = GetLastError();
                if (debugLog( WriteTime ) && (errorCode != ERROR_SUCCESS)) {
                    debugMessage( "GetFileInformationByHandle" ) << handleCode( handle)  << " ) failed with error : " << lasErrorString( errorCode ) << record;
                }
            }
            return FileTime();
        }
        // Register file access mode on file path
        wstring fileAccess( const wstring& fileName, FileAccessMode mode ) {
            auto fullFileName = fullName( fileName.c_str() );
            if (fullFileName != L"") {
                if (debugLog( FileAccesses )) debugRecord() << L"MonitorFiles - " << modeToString( mode ) << L" access by name on file " << fullFileName << record;
                recordEvent( fullFileName, mode, getLastWriteTime( fileName ) );
            }
            return fullFileName;
        }
        wstring fileAccess( const string& fileName, FileAccessMode mode ) {
            return fileAccess( widen( fileName ), mode );
        }
        // Register file access mode on (file) handle
        wstring fileAccess( HANDLE handle, FileAccessMode mode ) {
            auto fullFileName = fullName( handle );
            if (fullFileName != L"") {
                if (debugLog( FileAccesses )) debugRecord() << L"MonitorFiles - " << modeToString( mode ) << L" access by handle " << handleCode( handle) << " on file " << fullFileName << record;
                recordEvent( fullFileName, mode, getLastWriteTime( handle ) );
            }
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