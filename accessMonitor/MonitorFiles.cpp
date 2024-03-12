#include "MonitorFiles.h"
#include "FileAccess.h"
#include "MonitorLogging.h"
#include "Patch.h"

#include <windows.h>
#include <winternl.h>
#include <filesystem>

using namespace std;
using namespace std::filesystem;

// ToDo: Add function calling convention to all externals
// ToDo: Last write time on directories

namespace AccessMonitor {

    namespace {

        FileAccessMode requestedAccessMode( DWORD desiredAccess );

        wstring fileAccess( const string& fileName, FileAccessMode mode );
        wstring fileAccess( const wstring& fileName, FileAccessMode mode );
        wstring fileAccess( HANDLE handle, FileAccessMode mode = AccessNone );
        wstring fileAccess( HANDLE handle, const OBJECT_ATTRIBUTES* attributes );

        inline uint64_t handleCode( HANDLE handle ) { return reinterpret_cast<uint64_t>( handle ); }

        typedef BOOL(*TypeCreateDirectoryA)(LPCSTR,LPSECURITY_ATTRIBUTES);
        BOOL PatchCreateDirectoryA(
            LPCSTR                pathName,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto function = reinterpret_cast<TypeCreateDirectoryA>(original( (PatchFunction)PatchCreateDirectoryA ));
            BOOL created = function( pathName, securityAttributes );
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CreateDirectoryA( " << pathName << L", ... ) -> " << created << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CreateDirectoryW( " << pathName << L", ... ) -> " << created << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CreateDirectoryExA( " << newDirectory << L", ... ) -> " << created << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CreateDirectoryExA( " << newDirectory << L", ... ) -> " << created << record;
            fileAccess( newDirectory, AccessWrite );
            return created;
        }
        typedef BOOL(*TypeRemoveDirectoryA)(LPCSTR);
        BOOL PatchRemoveDirectoryA(
            LPCSTR pathName
        ) {
            auto function = reinterpret_cast<TypeRemoveDirectoryA>(original( (PatchFunction)PatchRemoveDirectoryA ));
            BOOL removed = function( pathName );
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - RemoveDirectoryA( " << pathName << L", ... ) -> " << removed << record;
            fileAccess( pathName, AccessDelete );
            return removed;
        }
        typedef BOOL(*TypeRemoveDirectoryW)(LPCWSTR);
        BOOL PatchRemoveDirectoryW(
            LPCWSTR pathName
        ) {
            auto function = reinterpret_cast<TypeRemoveDirectoryW>(original( (PatchFunction)PatchRemoveDirectoryW ));
            BOOL removed = function( pathName );
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - RemoveDirectoryW( " << pathName << L", ... ) -> " << removed << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CreateFileA( " << fileName << L", ... ) -> " << handleCode( handle ) << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CreateFileW( " << fileName << L", ... ) -> " << handleCode( handle ) << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CreateFile2( " << fileName << L", ... ) -> " << handleCode( handle ) << record;
            fileAccess( handle, requestedAccessMode( dwDesiredAccess ) );
            return handle;
        }
        typedef BOOL(*TypeDeleteFIleA)(LPCSTR);
        BOOL PatchDeleteFileA( LPCSTR fileName ) {
            TypeDeleteFIleA function = reinterpret_cast<TypeDeleteFIleA>(original( (PatchFunction)PatchDeleteFileA ));
            BOOL deleted = function( fileName );
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - DeleteFileA( " << fileName << L", ... ) -> " << deleted << record;
            fileAccess( fileName, AccessDelete );
            return deleted;
        }
        typedef BOOL(*TypeDeleteFIleW)(LPCWSTR);
        BOOL PatchDeleteFileW( LPCWSTR fileName ) {
            auto function = reinterpret_cast<TypeDeleteFIleW>(original( (PatchFunction)PatchDeleteFileW ));
            BOOL deleted = function( fileName );
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - DeleteFileW( " << fileName << L", ... ) -> " << deleted << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CopyFileA( " << existingFileName << L", " << newFileName << L", ... ) -> " << copied << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CopyFileW( " << existingFileName << L", " << newFileName << L", ... ) -> " << copied << record;
            fileAccess( existingFileName, (AccessRead | AccessDelete) );
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CopyFileExA( " << existingFileName << L", " << newFileName << L", ... ) -> " << copied << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CopyFileExW( " << existingFileName << L", " << newFileName << L", ... ) -> " << copied << record;
            fileAccess( existingFileName, AccessRead );
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - CopyFile2( " << existingFileName << L", " << newFileName << L", ... ) -> " << copied << record;
            fileAccess( existingFileName, AccessRead );
            fileAccess( newFileName, AccessWrite );
            return copied;
        }
        typedef BOOL(*TypeMoveFileA)(LPCSTR,LPCSTR);
        BOOL PatchMoveFileA(
            LPCSTR existingFileName,
            LPCSTR newFileName
        ) {
            auto function = reinterpret_cast<TypeMoveFileA>(original( (PatchFunction)PatchMoveFileA ));
            BOOL moved = function( existingFileName, newFileName );
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - MoveFileA( " << existingFileName << L", " << newFileName << L", ... ) -> " << moved << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - MoveFileW( " << existingFileName << L", " << newFileName << L", ... ) -> " << moved << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - MoveFileWithProgressA( " << existingFileName << L", " << newFileName << L", ... ) -> " << moved << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - MoveFileWithProgressW( " << existingFileName << L", " << newFileName << L", ... ) -> " << moved << record;
            fileAccess( existingFileName, AccessDelete );
            fileAccess( newFileName, AccessWrite );
            // ToDo: Record last write time on newFileName (wait until copy complete!)
            return moved;
        }
        typedef HANDLE(*TypeFindFirstFileA)(LPCSTR,LPWIN32_FIND_DATAA);
        HANDLE PatchFindFirstFileA(
            LPCSTR             fileName,
            LPWIN32_FIND_DATAA lpFindFileData
        ) {
            auto function = reinterpret_cast<TypeFindFirstFileA>(original( (PatchFunction)PatchFindFirstFileA ));
            HANDLE handle = function( fileName, lpFindFileData );
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - FindFirstFileA( " << fileName << L", ... )" << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - FindFirstFileW( " << fileName << L", ... )" << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - FindFirstFileExA( " << fileName << L", ... )" << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - FindFirstFileExW( " << fileName << L", ... )" << record;
            fileAccess( fileName, AccessRead );
            return handle;
        }
        typedef DWORD(*TypeGetFileAttributesA)(LPCSTR);
        DWORD PatchGetFileAttributesA(
            LPCSTR fileName
        ) {
            auto function = reinterpret_cast<TypeGetFileAttributesA>(original( (PatchFunction)PatchGetFileAttributesA ));
            DWORD attributes = function( fileName );
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - GetFileAttributesA( " << fileName << L", ... ) -> " << attributes << record;
            fileAccess( fileName, AccessRead );
            return attributes;
        }
        typedef DWORD(*TypeGetFileAttributesW)(LPCWSTR);
        DWORD PatchGetFileAttributesW(
            LPCWSTR fileName
        ) {
            auto function = reinterpret_cast<TypeGetFileAttributesW>(original( (PatchFunction)PatchGetFileAttributesW ));
            DWORD attributes = function( fileName );
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - GetFileAttributesW( " << fileName << L", ... ) -> " << attributes << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - GetFileAttributesExA( " << fileName << L", ... ) -> " << attributes << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - GetFileAttributesExW( " << fileName << L", ... ) -> " << attributes << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - SetFileAttributesA( " << fileName << L", ... ) -> " << set << record;
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
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - SetFileAttributesW( " << fileName << L", ... ) -> " << set << record;
            fileAccess( fileName, AccessWrite );
            return set;
        }
        typedef BOOL(*TypeCloseHandle)(HANDLE);
        BOOL PatchCloseHandle( HANDLE handle ) {
            auto function = reinterpret_cast<TypeCloseHandle>(original( (PatchFunction)PatchCloseHandle ));
            // Record last write time when closing file opened for write
            wstring fileName = fileAccess( handle );
            BOOL closed = function( handle );
            if (monitorLog( PatchExecution ) && (fileName != L"")) monitorLog() << L"MonitorFiles - CloseHandle( " << handleCode( handle ) << L") on " << fileName.c_str() << L" -> " << (closed ? L"closed" : L"failed") << record;
            return closed;
        }
        typedef NTSTATUS(*TypeNtCreateFile)(HANDLE*,ACCESS_MASK,OBJECT_ATTRIBUTES*,IO_STATUS_BLOCK*,LARGE_INTEGER*,ULONG,ULONG,ULONG,ULONG,void*,ULONG);
        NTSTATUS PatchNtCreateFile(
            HANDLE*            FileHandle,
            ACCESS_MASK        DesiredAccess,
            OBJECT_ATTRIBUTES* ObjectAttributes,
            IO_STATUS_BLOCK*   IoStatusBlock,
            LARGE_INTEGER*     AllocationSize,
            ULONG              FileAttributes,
            ULONG              ShareAccess,
            ULONG              CreateDisposition,
            ULONG              CreateOptions,
            void*              EaBuffer,
            ULONG              EaLength
        ) {
            auto function = reinterpret_cast<TypeNtCreateFile>(original( (PatchFunction)PatchNtCreateFile ));
            unpatchFunction( (PatchFunction)PatchNtCreateFile );
            auto status = function( FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength );
            // wstring fileName = fileAccess( *FileHandle, ObjectAttributes );
            // if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - NtCreateFile( ... " << fileName << " ... ) -> " << status << record;
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - NtCreateFile( ... ) -> " << status << record;
            repatchFunction( (PatchFunction)PatchNtCreateFile );
            return status;
        }
        typedef NTSTATUS(*TypeNtOpenFile)(HANDLE*,ACCESS_MASK,OBJECT_ATTRIBUTES*,IO_STATUS_BLOCK*,ULONG,ULONG);
        NTSTATUS PatchNtOpenFile(
            HANDLE*            FileHandle,
            ACCESS_MASK        DesiredAccess,
            OBJECT_ATTRIBUTES* ObjectAttributes,
            IO_STATUS_BLOCK*   IoStatusBlock,
            ULONG              ShareAccess,
            ULONG              OpenOptions
        ) {
            auto function = reinterpret_cast<TypeNtOpenFile>(original( (PatchFunction)PatchNtOpenFile ));
            unpatchFunction( (PatchFunction)PatchNtOpenFile );
            auto status = function( FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions );
            // wstring fileName = fileAccess( *FileHandle, ObjectAttributes );
            // if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - NtOpenFile( ... " << fileName << " ... ) -> " << status << record;
            if (monitorLog( PatchExecution )) monitorLog() << L"MonitorFiles - NtOpenFile( ... ) -> " << status << record;
            repatchFunction( (PatchFunction)PatchNtOpenFile );
            return status;
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

        // Determine access modes to opened file
        FileAccessMode accessMode( HANDLE handle ) {
            FileAccessMode mode( AccessNone );
            uint32_t flags;
            DWORD infoError = GetFileInformationByHandleEx( handle, (FILE_INFO_BY_HANDLE_CLASS)8, &flags, sizeof( flags ) );
            if (infoError == ERROR_SUCCESS) {
                if ((FILE_READ_DATA & flags) != 0) mode |= AccessRead;
                if ((FILE_WRITE_DATA & flags) != 0) mode |= AccessWrite;
                if ((FILE_APPEND_DATA & flags) != 0) mode |= AccessWrite;
            }
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
        // ToDo: Fix this function, required for patched NT functions...
        wstring fullName( const OBJECT_ATTRIBUTES* attributes ) {
            UNICODE_STRING* unicodeString = attributes->ObjectName;
            wstring name( unicodeString->Buffer, unicodeString->Length );
            path filePath( name );
            if (attributes->RootDirectory != nullptr) {
                filePath = fullName( attributes->RootDirectory );
                filePath /= name;
            }
            return filePath.wstring();
        }

        // Retrieve last write time on a file (handle)
        // Note that Windows file times have (limited) millisecond resolution
        // Will return default time (i.e., 1970-01-01) if handle does not refer to a file.
        FileTime getLastWriteTime( HANDLE handle ) {
            FileTime time;
            FILETIME fileTime;
            if (GetFileTime( handle, nullptr, nullptr, &fileTime )) {
                // Convert Windows FILETIME to C++ chrono::time_point as system clock time
                SYSTEMTIME systemFileTime;
                if (FileTimeToSystemTime( &fileTime, &systemFileTime )) {
                    tm t;
                    t.tm_year = (systemFileTime.wYear - 1900);
                    t.tm_mon = (systemFileTime.wMonth - 1);
                    t.tm_mday = systemFileTime.wDay;
                    t.tm_hour = systemFileTime.wHour;
                    t.tm_min = systemFileTime.wMinute;
                    t.tm_sec = systemFileTime.wSecond;
                    t.tm_isdst = 0;
                    time = chrono::system_clock::from_time_t( mktime( &t ) );
                    time += (systemFileTime.wMilliseconds * chrono::milliseconds(1) );
                }
            }
            return time;
        }

        // Get last write time of a (named) file
        FileTime getLastWriteTime( const wstring& fileName ) {
            FileTime time;
            wstring fullFileName = fullName( fileName.c_str() );
            auto createFile = reinterpret_cast<TypeCreateFileW>(original( (PatchFunction)PatchCreateFileW ));
            HANDLE handle = createFile( fullFileName.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
            DWORD createError = GetLastError();
            if (createError == ERROR_SUCCESS) {
                time = getLastWriteTime( handle );
                if (monitorLog( WriteTime )) monitorLog() << L"MonitorFiles - Last write time " << time << L" for access on " << fileName << record;
                auto closeFile = reinterpret_cast<TypeCloseHandle>(original( (PatchFunction)PatchCloseHandle ));
                closeFile( handle );
            }
            return time;
        }
        FileTime getLastWriteTime( const string& fileName ) {
            return getLastWriteTime( widen( fileName ) );
        }

        void recordFileEvent( const wstring& file, FileAccessMode mode, const FileTime& time ) {
            if (SessionRecording()) eventRecord() << file << L" [ " << time << L" ] " << modeToString( mode ) << record;
        }

        // Register file access mode on file (path)
        wstring fileAccess( const wstring& fileName, FileAccessMode mode ) {
            DWORD error = GetLastError();
            auto fullFileName = fullName( fileName.c_str() );
            if (monitorLog( FileAccesses )) monitorLog() << L"MonitorFiles - " << modeToString( mode ) << L" access by name on file " << fullFileName << record;
            if (fullFileName != L"") recordFileEvent( fullFileName, mode, getLastWriteTime( fileName ) );
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
                if (mode == AccessNone) mode = accessMode( handle );
                if (monitorLog( FileAccesses )) monitorLog() << L"MonitorFiles - " << modeToString( mode ) << L" access by handle on file " << fullFileName << record;
                if (fullFileName != L"") recordFileEvent( fullFileName, mode, getLastWriteTime( handle ) );
            }
            SetLastError( error );
            return fullFileName;
        }
        wstring fileAccess( HANDLE handle, const OBJECT_ATTRIBUTES* attributes ) {
            DWORD error = GetLastError();
            auto fullFileName = fullName( attributes );
            if (fullFileName != L"") {
                FileAccessMode  mode = accessMode( handle );
                if (monitorLog( FileAccesses )) monitorLog() << L"MonitorFiles - " << modeToString( mode ) << L" access by handle on file " << fullFileName << record;
                if (fullFileName != L"") recordFileEvent( fullFileName, mode, getLastWriteTime( handle ) );
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
        registerPatch( "CreateFile2", (PatchFunction)PatchCreateFile2 );
        registerPatch( "DeleteFileA", (PatchFunction)PatchDeleteFileA );
        registerPatch( "DeleteFileW", (PatchFunction)PatchDeleteFileW );
        registerPatch( "CopyFileA", (PatchFunction)PatchCopyFileA );
        registerPatch( "CopyFileW", (PatchFunction)PatchCopyFileW );
        registerPatch( "CopyFileExA", (PatchFunction)PatchCopyFileExA );
        registerPatch( "CopyFileExW", (PatchFunction)PatchCopyFileExW );
        registerPatch( "CopyFile2", (PatchFunction)PatchCopyFile2 );
        registerPatch( "MoveFileA", (PatchFunction)PatchMoveFileA );
        registerPatch( "MoveFileW", (PatchFunction)PatchMoveFileW );
        registerPatch( "MoveFileWithProgressA", (PatchFunction)PatchMoveFileWithProgressA );
        registerPatch( "MoveFileWithProgressW", (PatchFunction)PatchMoveFileWithProgressW );
        registerPatch( "FindFirstFileA", (PatchFunction)PatchFindFirstFileA );
        registerPatch( "FindFirstFileW", (PatchFunction)PatchFindFirstFileW );
        registerPatch( "FindFirstFileExA", (PatchFunction)PatchFindFirstFileExA );
        registerPatch( "FindFirstFileExW", (PatchFunction)PatchFindFirstFileExW );
        registerPatch( "GetFileAttributesA", (PatchFunction)PatchGetFileAttributesA );
        registerPatch( "GetFileAttributesW", (PatchFunction)PatchGetFileAttributesW );
        registerPatch( "GetFileAttributesExA", (PatchFunction)PatchGetFileAttributesExA );
        registerPatch( "GetFileAttributesExW", (PatchFunction)PatchGetFileAttributesExW );
        registerPatch( "SetFileAttributesA", (PatchFunction)PatchSetFileAttributesA );
        registerPatch( "SetFileAttributesW", (PatchFunction)PatchSetFileAttributesW );
        registerPatch( "CloseHandle", (PatchFunction)PatchCloseHandle );
        registerPatch( "NtCreateFile", (PatchFunction)PatchNtCreateFile );
        registerPatch( "NtOpenFile", (PatchFunction)PatchNtOpenFile );
    }
    void unregisterFileAccess() {
        unregisterPatch( "CreateDirectoryA" );
        unregisterPatch( "CreateDirectoryW" );
        unregisterPatch( "RemoveDirectoryA" );
        unregisterPatch( "RemoveDirectoryW" );
        unregisterPatch( "CreateFileA" );
        unregisterPatch( "CreateFileW" );
        unregisterPatch( "CreateFile2" );
        unregisterPatch( "DeleteFileA" );
        unregisterPatch( "DeleteFileW" );
        unregisterPatch( "CopyFileA" );
        unregisterPatch( "CopyFileW" );
        unregisterPatch( "CopyFileExA" );
        unregisterPatch( "CopyFileExW" );
        unregisterPatch( "CopyFile2" );
        unregisterPatch( "MoveFileA" );
        unregisterPatch( "MoveFileW" );
        unregisterPatch( "MoveFileWithProgressA" );
        unregisterPatch( "MoveFileWithProgressW" );
        unregisterPatch( "FindFirstFileA" );
        unregisterPatch( "FindFirstFileW" );
        unregisterPatch( "FindFirstFileExA" );
        unregisterPatch( "FindFirstFileExW" );
        unregisterPatch( "GetFileAttributesA" );
        unregisterPatch( "GetFileAttributesW" );
        unregisterPatch( "GetFileAttributesExA" );
        unregisterPatch( "GetFileAttributesExW" );
        unregisterPatch( "SetFileAttributesA" );
        unregisterPatch( "SetFileAttributesW" );
        unregisterPatch( "CloseHandle" );
        unregisterPatch( "NtCreateFile" );
        unregisterPatch( "NtOpenFile" );
    }

} // namespace AccessMonitor