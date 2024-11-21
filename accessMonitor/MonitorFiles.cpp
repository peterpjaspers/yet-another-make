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

// ToDo: Implenent for Linux and MacOS, current implementation is Windows only...

namespace AccessMonitor {

    namespace {

        class FileGuard : public MonitorGuard {
        public:
            FileGuard() : MonitorGuard( Session::fileAccess() ) {}
        };

        FileAccessMode requestedAccessMode( DWORD desiredAccess );
        FileAccessMode requestedAccessMode( UINT desiredAccess );

        wstring fullName( HANDLE handle );
        wstring fullName( const wchar_t* fileName );
        wstring fullName( const char* fileName );

        wstring fileAccessFull( const wstring& fullFileName, FileAccessMode mode, bool success = true );
        wstring fileAccess( const wstring& fileName, FileAccessMode mode, bool success = true );
        wstring fileAccess( const string& fileName, FileAccessMode mode, bool success = true );

        inline uint64_t handleCode( HANDLE handle ) { return reinterpret_cast<uint64_t>( handle ); }

        inline wostream& debugMessage( const char* function ) {
            return debugRecord() << L"MonitorFiles - " << function << "( ";
        }
        inline wostream& debugMessage( const char* function, bool success ) {
            DWORD errorCode = GetLastError();
            if (!success && (errorCode != ERROR_SUCCESS)) debugRecord() << L"MonitorFiles - " << function << L" failed with error : " << errorString( errorCode ) << record;
            return debugMessage( function );
        }
        inline wostream& debugMessage( const char* function, HANDLE handle ) {
            return debugMessage( function, (handle != INVALID_HANDLE_VALUE) );
        }
        inline bool successful( bool success, unsigned long error ) {
            return( success || (error == ERROR_SUCCESS) );
        }

        BOOL PatchCreateDirectoryA(
            LPCSTR                pathName,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto created = patchOriginal( PatchCreateDirectoryA )( pathName, securityAttributes );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryA", created ) << pathName << L", ... )" << record;
                fileAccess( pathName, AccessWrite, successful( created, guard.error() ) );
            }
            return created;
        }
        BOOL PatchCreateDirectoryW(
            LPCWSTR                pathName,
            LPSECURITY_ATTRIBUTES  securityAttributes
        ) {
            auto created = patchOriginal( PatchCreateDirectoryW )( pathName, securityAttributes );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryW", created ) << pathName << L", ... )" << record;
                fileAccess( pathName, AccessWrite, successful( created, guard.error() ) );
            }
            return created;
        }
        BOOL PatchCreateDirectoryExA(
            LPCSTR                templateDirectory,
            LPCSTR                newDirectory,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto created = patchOriginal( PatchCreateDirectoryExA )( templateDirectory, newDirectory, securityAttributes );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryExA", created ) << newDirectory << L", ... )" << record;
                fileAccess( newDirectory, AccessWrite, successful( created, guard.error() ) );
            }
            return created;
        }
        BOOL PatchCreateDirectoryExW(
            LPCWSTR                templateDirectory,
            LPCWSTR                newDirectory,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto created = patchOriginal( PatchCreateDirectoryExW )( templateDirectory, newDirectory, securityAttributes );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateDirectoryExA", created ) << newDirectory << L", ... )" << record;
                fileAccess( newDirectory, AccessWrite, successful( created, guard.error() ) );
            }
            return created;
        }
        BOOL PatchRemoveDirectoryA(
            LPCSTR pathName
        ) {
            auto removed = patchOriginal( PatchRemoveDirectoryA )( pathName );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "RemoveDirectoryA", removed ) << pathName << L", ... )" << record;
                fileAccess( pathName, AccessDelete, successful( removed, guard.error() ) );
            }
            return removed;
        }
        BOOL PatchRemoveDirectoryW(
            LPCWSTR pathName
        ) {
            auto removed = patchOriginal( PatchRemoveDirectoryW )( pathName );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "RemoveDirectoryW", removed ) << pathName << L", ... )" << record;
                fileAccess( pathName, AccessDelete, successful( removed, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateFileA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, requestedAccessMode( dwDesiredAccess ), (handle != INVALID_HANDLE_VALUE) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateFileW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, requestedAccessMode( dwDesiredAccess ), (handle != INVALID_HANDLE_VALUE) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateFileTransactedA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, requestedAccessMode( dwDesiredAccess ), (handle != INVALID_HANDLE_VALUE) );

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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateFileTransactedW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, requestedAccessMode( dwDesiredAccess ), (handle != INVALID_HANDLE_VALUE) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateFile2", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, requestedAccessMode( dwDesiredAccess ), (handle != INVALID_HANDLE_VALUE) );
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
            FileGuard guard;
            if ( guard() ) {
                auto fileName = fullName( hOriginalFile );
                if (debugLog( PatchExecution )) debugMessage( "ReOpenFile", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, requestedAccessMode( dwDesiredAccess ), (handle != INVALID_HANDLE_VALUE) );
            }
            return handle;
        }
        HFILE PatchOpenFile(
            LPCSTR     fileName,
            LPOFSTRUCT lpReOpenBuff,
            UINT       uStyle
        ) {
            auto handle = patchOriginal( PatchOpenFile )( fileName, lpReOpenBuff, uStyle );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "OpenFile", (handle != HFILE_ERROR) ) << fileName << L", ... ) -> " << handle << record;
                fileAccess( fileName, requestedAccessMode( uStyle ), (handle != HFILE_ERROR) );
            }
            return handle;

        }
        BOOL PatchDeleteFileA( LPCSTR fileName ) {
            auto deleted = patchOriginal( PatchDeleteFileA )( fileName );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "DeleteFileA", deleted ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessDelete, successful( deleted, guard.error() ) );
            }
            return deleted;
        }
        BOOL PatchDeleteFileW( LPCWSTR fileName ) {
            auto deleted = patchOriginal( PatchDeleteFileW )( fileName );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "DeleteFileW", deleted ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessDelete, successful( deleted, guard.error() ) );
            }
            return deleted;
        }
        BOOL PatchDeleteFileTransactedA( LPCSTR fileName, HANDLE hTransaction ) {
            auto deleted = patchOriginal( PatchDeleteFileTransactedA )( fileName, hTransaction );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "DeleteFileTransactedA", deleted ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessDelete, successful( deleted, guard.error() ) );
            }
            return deleted;
        }
        BOOL PatchDeleteFileTransactedW( LPCWSTR fileName, HANDLE hTransaction ) {
            auto deleted = patchOriginal( PatchDeleteFileTransactedW )( fileName, hTransaction );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "DeleteFileTransactedW", deleted ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessDelete, successful( deleted, guard.error() ) );
            }
            return deleted;
        }
        BOOL PatchCopyFileA(
            LPCSTR existingFileName,
            LPCSTR newFileName,
            BOOL   failIfExists
        ) {
            auto copied = patchOriginal( PatchCopyFileA )( existingFileName, newFileName, failIfExists );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileA", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead, successful( copied, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( copied, guard.error() ) );
            }
            return copied;
        }
        BOOL PatchCopyFileW(
            LPCWSTR existingFileName,
            LPCWSTR newFileName,
            BOOL    failIfExists
        ) {
            auto copied = patchOriginal( PatchCopyFileW )( existingFileName, newFileName, failIfExists );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileW", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead, successful( copied, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( copied, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileExA", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead, successful( copied, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( copied, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileExW", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead, successful( copied, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( copied, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileTransactedA", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead, successful( copied, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( copied, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFileTransactedW", copied ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead, successful( copied, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( copied, guard.error() ) );
            }
            return copied;
        }
        HRESULT PatchCopyFile2(
            PCWSTR                         existingFileName,
            PCWSTR                         newFileName,
            COPYFILE2_EXTENDED_PARAMETERS* pExtendedParameters
        ) {
            auto result = patchOriginal( PatchCopyFile2 )( existingFileName, newFileName, pExtendedParameters );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CopyFile2", (result == S_OK)) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessRead, (result == S_OK) );
                fileAccess( newFileName, AccessWrite, (result == S_OK) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "ReplaceFileA", replaced ) << lpReplacedFileName << L", " << lpReplacementFileName << L", ... )" << record;
                fileAccess( lpReplacementFileName, AccessRead, successful( replaced, guard.error() ) );
                fileAccess( lpReplacedFileName, AccessWrite, successful( replaced, guard.error() ) );
                if (lpBackupFileName!= nullptr) fileAccess( lpBackupFileName, AccessWrite, successful( replaced, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "ReplaceFileW", replaced ) << lpReplacedFileName << L", " << lpReplacementFileName << L", ... )" << record;
                fileAccess( lpReplacementFileName, AccessRead, successful( replaced, guard.error() ) );
                fileAccess( lpReplacedFileName, AccessWrite, successful( replaced, guard.error() ) );
                if (lpBackupFileName!= nullptr) fileAccess( lpBackupFileName, AccessWrite, successful( replaced, guard.error() ) );
            }
            return replaced;
        }
        BOOL PatchMoveFileA(
            LPCSTR existingFileName,
            LPCSTR newFileName
        ) {
            auto moved = patchOriginal( PatchMoveFileA )( existingFileName, newFileName );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete, successful( moved, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( moved, guard.error() ) );
            }
            return moved;
        }       
        BOOL PatchMoveFileW(
            LPCWSTR existingFileName,
            LPCWSTR newFileName
        ) {
            auto moved = patchOriginal( PatchMoveFileW )( existingFileName, newFileName );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete, successful( moved, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( moved, guard.error() ) );
            }
            return moved;
        }
        BOOL PatchMoveFileExA(
            LPCSTR  existingFileName,
            LPCSTR  newFileName,
            DWORD   flags
        ) {
            auto moved = patchOriginal( PatchMoveFileExA )( existingFileName, newFileName, flags );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileExA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete, successful( moved, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( moved, guard.error() ) );
            }
            return moved;
        }       
        BOOL PatchMoveFileExW(
            LPCWSTR existingFileName,
            LPCWSTR newFileName,
            DWORD   flags
        ) {
            auto moved = patchOriginal( PatchMoveFileExW )( existingFileName, newFileName, flags );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileExW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete, successful( moved, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( moved, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileWithProgressA", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete, successful( moved, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( moved, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "MoveFileWithProgressW", moved ) << existingFileName << L", " << newFileName << L", ... )" << record;
                fileAccess( existingFileName, AccessDelete, successful( moved, guard.error() ) );
                fileAccess( newFileName, AccessWrite, successful( moved, guard.error() ) );
            }
            return moved;
        }
        HANDLE PatchFindFirstFileA(
            LPCSTR             fileName,
            LPWIN32_FIND_DATAA lpFindFileData
        ) {
            auto handle = patchOriginal( PatchFindFirstFileA )( fileName, lpFindFileData );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead, (handle != INVALID_HANDLE_VALUE) );
            }
            return handle;
        }        
        HANDLE PatchFindFirstFileW(
            LPCWSTR            fileName,
            LPWIN32_FIND_DATAW lpFindFileData
        ) {
            auto handle = patchOriginal( PatchFindFirstFileW )( fileName, lpFindFileData );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead, (handle != INVALID_HANDLE_VALUE) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileExA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead, (handle != INVALID_HANDLE_VALUE) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileExW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead, (handle != INVALID_HANDLE_VALUE) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileTransactedA", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead, (handle != INVALID_HANDLE_VALUE) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "FindFirstFileTransactedW", handle ) << fileName << L", ... ) -> " << handleCode( handle ) << record;
                fileAccess( fileName, AccessRead, (handle != INVALID_HANDLE_VALUE) );
            }
            return handle;
        }
        DWORD PatchGetFileAttributesA(
            LPCSTR fileName
        ) {
            auto attributes = patchOriginal( PatchGetFileAttributesA )( fileName );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesA", (attributes != INVALID_FILE_ATTRIBUTES) ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead, (attributes != INVALID_FILE_ATTRIBUTES) );
            }
            return attributes;
        }
        DWORD PatchGetFileAttributesW(
            LPCWSTR fileName
        ) {
            auto attributes = patchOriginal( PatchGetFileAttributesW )( fileName );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesW", (attributes != INVALID_FILE_ATTRIBUTES) ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead, (attributes != INVALID_FILE_ATTRIBUTES) );
            }
            return attributes;
        }
        BOOL PatchGetFileAttributesExA(
            LPCSTR                 fileName,
            GET_FILEEX_INFO_LEVELS fInfoLevelId,
            LPVOID                 lpFileInformation
        ) {
            auto got = patchOriginal( PatchGetFileAttributesExA )( fileName, fInfoLevelId, lpFileInformation );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesExA", got ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead, successful( got, guard.error() ) );
            }
            return got;
        }
        BOOL PatchGetFileAttributesExW(
            LPCWSTR                 fileName,
            GET_FILEEX_INFO_LEVELS fInfoLevelId,
            LPVOID                 lpFileInformation

        ) {
            auto got = patchOriginal( PatchGetFileAttributesExW )( fileName, fInfoLevelId, lpFileInformation );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesExW", got ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead, successful( got, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesTransactedA", got ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead, successful( got, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "GetFileAttributesTransactedW", got ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessRead, successful( got, guard.error() ) );
            }
            return got;
        }
        BOOL PatchSetFileAttributesA(
            LPCSTR fileName,
            DWORD  dwFileAttributes
        ) {
            auto set = patchOriginal( PatchSetFileAttributesA )( fileName, dwFileAttributes );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "PatchSetFileAttributesA", set ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessWrite, successful( set, guard.error() ) );
            }
            return set;
        }
        BOOL PatchSetFileAttributesW(
            LPCWSTR fileName,
            DWORD  dwFileAttributes
        ) {
            auto set = patchOriginal( PatchSetFileAttributesW )( fileName, dwFileAttributes );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "PatchSetFileAttributesW", set ) << fileName << L", ... )" << record;
                fileAccess( fileName, AccessWrite, successful( set, guard.error() ) );
            }
            return set;
        }
        BOOL PatchCreateHardLinkA(
            LPCSTR                lpFileName,
            LPCSTR                lpExistingFileName,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes
        ) {
            auto created = patchOriginal( PatchCreateHardLinkA )( lpFileName, lpExistingFileName, lpSecurityAttributes );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkA", created )  << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
                fileAccess( lpFileName, AccessWrite, successful( created, guard.error() ) );
                fileAccess( lpExistingFileName, AccessRead, successful( created, guard.error() ) );
            }
            return created;
        }
        BOOL PatchCreateHardLinkW(
            LPCWSTR                lpFileName,
            LPCWSTR                lpExistingFileName,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes
        ) {
            auto created = patchOriginal( PatchCreateHardLinkW )( lpFileName, lpExistingFileName, lpSecurityAttributes );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkW", created )  << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
                fileAccess( lpFileName, AccessWrite, successful( created, guard.error() ) );
                fileAccess( lpExistingFileName, AccessRead, successful( created, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkTransactedA", created ) << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
                fileAccess( lpFileName, AccessWrite, successful( created, guard.error() ) );
                fileAccess( lpExistingFileName, AccessRead, successful( created, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateHardLinkTransactedW", created ) << lpFileName << L", " << lpExistingFileName << ",  ... )" << record;
                fileAccess( lpFileName, AccessWrite, successful( created, guard.error() ) );
                fileAccess( lpExistingFileName, AccessRead, successful( created, guard.error() ) );
            }
            return created;
        }
        BOOLEAN PatchCreateSymbolicLinkA(
            LPCSTR lpSymlinkFileName,
            LPCSTR lpTargetFileName,
            DWORD  dwFlags
        ) {
            auto created = patchOriginal( PatchCreateSymbolicLinkA )( lpSymlinkFileName, lpTargetFileName, dwFlags );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkA", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
                fileAccess( lpSymlinkFileName, AccessWrite, successful( created, guard.error() ) );
                fileAccess( lpTargetFileName, AccessRead, successful( created, guard.error() ) );
            }
            return created;
        }
        BOOLEAN PatchCreateSymbolicLinkW(
            LPCWSTR lpSymlinkFileName,
            LPCWSTR lpTargetFileName,
            DWORD   dwFlags
        ) {
            auto created = patchOriginal( PatchCreateSymbolicLinkW )( lpSymlinkFileName, lpTargetFileName, dwFlags );
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkW", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
                fileAccess( lpSymlinkFileName, AccessWrite, successful( created, guard.error() ) );
                fileAccess( lpTargetFileName, AccessRead, successful( created, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkTransactedA", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
                fileAccess( lpSymlinkFileName, AccessWrite, successful( created, guard.error() ) );
                fileAccess( lpTargetFileName, AccessRead, successful( created, guard.error() ) );
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
            FileGuard guard;
            if ( guard() ) {
                if (debugLog( PatchExecution )) debugMessage( "CreateSymbolicLinkTransactedW", created ) << lpSymlinkFileName << L", " << lpTargetFileName << L", " << dwFlags << " )" << record;
                fileAccess( lpSymlinkFileName, AccessWrite, successful( created, guard.error() ) );
                fileAccess( lpTargetFileName, AccessRead, successful( created, guard.error() ) );
            }
            return created;
        }
        BOOL PatchCloseHandle( HANDLE handle ) {
            FileGuard guard;
            if ( guard() ) {
                // Record last write time when closing an actual file opened for write (also for WithProgress calls)
                auto fileName = fullName( handle );
                fileAccessFull( fileName, AccessNone );
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
            return path( simplePath ).generic_wstring();
        }

        // Extract file name from handle to opened file
        // Will return empty string if handle does not refer to a file
        wstring fullName( HANDLE handle ) {
            wchar_t fileNameString[ MaxFileName ];
            if ( !GetFinalPathNameByHandleW( handle, fileNameString, MaxFileName, 0 ) ) return L"";
            return simplify(fileNameString);
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

        void recordEvent( const wstring& file, FileAccessMode mode, const FileTime& time, bool success ) {
            if (recordingEvents()) eventRecord() << file << L" [" << time << L"] " << fileAccessModeToString(mode) << " " << success << record;
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
                    debugMessage( "GetFileAttributesExW" ) << fileName << " ) failed with error : " << errorString( errorCode ) << record;
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
                    debugMessage( "GetFileInformationByHandle" ) << handleCode( handle)  << " ) failed with error : " << errorString( errorCode ) << record;
                }
            }
            return FileTime();
        }

        // Register file access mode on file path
        wstring fileAccessFull( const wstring& fullFileName, FileAccessMode mode, bool success ) {
            if (fullFileName != L"") {
                if (debugLog( FileAccesses )) debugRecord() << L"MonitorFiles - " << fileAccessModeToString( mode ) << L" access on file " << fullFileName << record;
                recordEvent( fullFileName, mode, getLastWriteTime( fullFileName ), success );
            }
            return fullFileName;
        }
        wstring fileAccess( const wstring& fileName, FileAccessMode mode, bool success ) {
            auto fullFileName = fullName( fileName.c_str() );
            return fileAccessFull( fullFileName, mode, success );
        }
        wstring fileAccess( const string& fileName, FileAccessMode mode, bool success ) {
            return fileAccess( widen( fileName ), mode, success );
        }

        struct Registration {
            const char* library;
            const char* name;
            PatchFunction patch;
        };

        const vector<Registration> registrations = {
            { "kernel32", "CreateDirectoryA", (PatchFunction)PatchCreateDirectoryA },
            { "kernel32", "CreateDirectoryW", (PatchFunction)PatchCreateDirectoryW },
            { "kernel32", "RemoveDirectoryA", (PatchFunction)PatchRemoveDirectoryA },
            { "kernel32", "RemoveDirectoryW", (PatchFunction)PatchRemoveDirectoryW },
            { "kernel32", "CreateFileA", (PatchFunction)PatchCreateFileA },
            { "kernel32", "CreateFileW", (PatchFunction)PatchCreateFileW },
            { "kernel32", "CreateFileTransactedA", (PatchFunction)PatchCreateFileTransactedA },
            { "kernel32", "CreateFileTransactedW", (PatchFunction)PatchCreateFileTransactedW },
            { "kernel32", "CreateFile2", (PatchFunction)PatchCreateFile2 },
            { "kernel32", "ReOpenFile", (PatchFunction)PatchReOpenFile },
            { "kernel32", "ReplaceFileA", (PatchFunction)PatchReplaceFileA },
            { "kernel32", "ReplaceFileW", (PatchFunction)PatchReplaceFileW },
            { "kernel32", "OpenFile", (PatchFunction)PatchOpenFile },
            { "kernel32", "CreateHardLinkA", (PatchFunction)PatchCreateHardLinkA },
            { "kernel32", "CreateHardLinkW", (PatchFunction)PatchCreateHardLinkW },
            { "kernel32", "CreateHardLinkTransactedA", (PatchFunction)PatchCreateHardLinkTransactedA },
            { "kernel32", "CreateHardLinkTransactedW", (PatchFunction)PatchCreateHardLinkTransactedW },
            { "kernel32", "CreateSymbolicLinkA", (PatchFunction)PatchCreateSymbolicLinkA },
            { "kernel32", "CreateSymbolicLinkW", (PatchFunction)PatchCreateSymbolicLinkW },
            { "kernel32", "CreateSymbolicLinkTransactedA", (PatchFunction)PatchCreateSymbolicLinkTransactedA },
            { "kernel32", "CreateSymbolicLinkTransactedW", (PatchFunction)PatchCreateSymbolicLinkTransactedW },
            { "kernel32", "DeleteFileA", (PatchFunction)PatchDeleteFileA },
            { "kernel32", "DeleteFileW", (PatchFunction)PatchDeleteFileW },
            { "kernel32", "DeleteFileTransactedA", (PatchFunction)PatchDeleteFileTransactedA },
            { "kernel32", "DeleteFileTransactedW", (PatchFunction)PatchDeleteFileTransactedW },
            { "kernel32", "CopyFileA", (PatchFunction)PatchCopyFileA },
            { "kernel32", "CopyFileW", (PatchFunction)PatchCopyFileW },
            { "kernel32", "CopyFileExA", (PatchFunction)PatchCopyFileExA },
            { "kernel32", "CopyFileExW", (PatchFunction)PatchCopyFileExW },
            { "kernel32", "CopyFileTransactedA", (PatchFunction)PatchCopyFileTransactedA },
            { "kernel32", "CopyFileTransactedW", (PatchFunction)PatchCopyFileTransactedW },
            { "kernel32", "CopyFile2", (PatchFunction)PatchCopyFile2 },
            { "kernel32", "MoveFileA", (PatchFunction)PatchMoveFileA },
            { "kernel32", "MoveFileW", (PatchFunction)PatchMoveFileW },
            { "kernel32", "MoveFileExA", (PatchFunction)PatchMoveFileExA },
            { "kernel32", "MoveFileExW", (PatchFunction)PatchMoveFileExW },
            { "kernel32", "MoveFileWithProgressA", (PatchFunction)PatchMoveFileWithProgressA },
            { "kernel32", "MoveFileWithProgressW", (PatchFunction)PatchMoveFileWithProgressW },
            { "kernel32", "FindFirstFileA", (PatchFunction)PatchFindFirstFileA },
            { "kernel32", "FindFirstFileW", (PatchFunction)PatchFindFirstFileW },
            { "kernel32", "FindFirstFileExA", (PatchFunction)PatchFindFirstFileExA },
            { "kernel32", "FindFirstFileExW", (PatchFunction)PatchFindFirstFileExW },
            { "kernel32", "FindFirstFileTransactedA", (PatchFunction)PatchFindFirstFileTransactedA },
            { "kernel32", "FindFirstFileTransactedW", (PatchFunction)PatchFindFirstFileTransactedW },
            { "kernel32", "GetFileAttributesA", (PatchFunction)PatchGetFileAttributesA },
            { "kernel32", "GetFileAttributesW", (PatchFunction)PatchGetFileAttributesW },
            { "kernel32", "GetFileAttributesExA", (PatchFunction)PatchGetFileAttributesExA },
            { "kernel32", "GetFileAttributesExW", (PatchFunction)PatchGetFileAttributesExW },
            { "kernel32", "GetFileAttributesTransactedA", (PatchFunction)PatchGetFileAttributesTransactedA },
            { "kernel32", "GetFileAttributesTransactedW", (PatchFunction)PatchGetFileAttributesTransactedW },
            { "kernel32", "SetFileAttributesA", (PatchFunction)PatchSetFileAttributesA },
            { "kernel32", "SetFileAttributesW", (PatchFunction)PatchSetFileAttributesW },
            { "kernel32", "CloseHandle", (PatchFunction)PatchCloseHandle },
        };

    } // namespace (anonymous)

    void registerFileAccess() {
        for ( const auto reg : registrations ) registerPatch( reg.library, reg.name, reg.patch );
    }
    void unregisterFileAccess() {
        for ( const auto reg : registrations ) unregisterPatch( reg.name );
    }

} // namespace AccessMonitor