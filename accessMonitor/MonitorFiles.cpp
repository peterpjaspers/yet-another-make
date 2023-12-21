#include "MonitorFiles.h"
#include "Monitor.h"
#include "Patch.h"
#include "Log.h"

#include <windows.h>
#include <filesystem>
#include <chrono>
#include <map>

// ToDo: Multi-thread safe implementation (in particular accessedFiles map)
// Not sure if we can tolerate a synchronization mutex on the map
// A possible approach is to maintain thread local results that are communicated when threads exit.
// This requires communication and synchronization only once per thread but will require a
// merge of all thread local results.

// ToDo: stream relevant parameters to patched functions (in particular file names)

// CreateHardLinkW, GetFileAttributesW, GetFullPathNameW, GetLongPathNameW, MoveFileExW, ReplaceFileW
// SearchPathW, FindFirstStreamW, FindNextStreamW, GetCompressedFileSizeW, GetFinalPathNameByHandleW

using namespace std;
using namespace filesystem;

namespace AccessMonitor {

    FileAccessState::FileAccessState() : accessedModes( AccessNone ) {}

    namespace {

        thread_local map< wstring, FileAccessState > accessedFiles;

        struct FileAccess {
            const wstring& fileName;
            FileAccessState& state;
            FileAccess() = delete;
            FileAccess( const wstring& f, FileAccessState& s ) : fileName( f ), state( s ) {}
        };

        FileAccessMode requestedAccessMode( DWORD desiredAccess );
        wstring modeString( FileAccessMode mode );
        FileAccessMode accessModes( HANDLE handle );

        wstring widen( const string& fileName );
        wstring fullName( HANDLE handle );
        wstring fullName( const char* fileName );
        wstring fullName( const wchar_t* fileName );
        wstring fullName( const wstring& fileName );

        FileAccess fileAccess( const string& fileName, FileAccessMode mode );
        FileAccess fileAccess( const wstring& fileName, FileAccessMode mode );

        void getLastWriteTime( HANDLE handle, FileTime& timePoint );
        void getLastWriteTime( const wstring& fileName, FileTime& timePoint );
        void getLastWriteTime( const wstring& fileName );

        typedef BOOL(*TypeCreateDirectoryA)(LPCSTR,LPSECURITY_ATTRIBUTES);
        BOOL PatchCreateDirectoryA(
            LPCSTR                pathName,
            LPSECURITY_ATTRIBUTES securityAttributes
        ) {
            auto function = reinterpret_cast<TypeCreateDirectoryA>(original( (PatchFunction)PatchCreateDirectoryA ));
            BOOL created = function( pathName, securityAttributes );
            if (logging( Terse )) log() << L"MonitorFiles - CreateDirectoryA( " << pathName << L", ... ) -> " << created << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - CreateDirectoryW( " << pathName << L", ... ) -> " << created << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - CreateDirectoryExA( " << newDirectory << L", ... ) -> " << created << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - CreateDirectoryExA( " << newDirectory << L", ... ) -> " << created << endLine;
            fileAccess( newDirectory, AccessWrite );
            return created;
        }
        typedef BOOL(*TypeRemoveDirectoryA)(LPCSTR);
        BOOL PatchRemoveDirectoryA(
            LPCSTR pathName
        ) {
            auto function = reinterpret_cast<TypeRemoveDirectoryA>(original( (PatchFunction)PatchRemoveDirectoryA ));
            BOOL removed = function( pathName );
            if (logging( Terse )) log() << L"MonitorFiles - RemoveDirectoryA( " << pathName << L", ... ) -> " << removed << endLine;
            fileAccess( pathName, AccessDelete );
            return removed;
        }
        typedef BOOL(*TypeRemoveDirectoryW)(LPCWSTR);
        BOOL PatchRemoveDirectoryW(
            LPCWSTR pathName
        ) {
            auto function = reinterpret_cast<TypeRemoveDirectoryW>(original( (PatchFunction)PatchRemoveDirectoryW ));
            BOOL removed = function( pathName );
            if (logging( Terse )) log() << L"MonitorFiles - RemoveDirectoryW( " << pathName << L", ... ) -> " << removed << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - CreateFileA( " << fileName << L", ... ) -> " << handle << endLine;
            auto mode = requestedAccessMode( dwDesiredAccess );
            auto access = fileAccess( fileName, mode );
            if ( mode == AccessRead ) getLastWriteTime( access.fileName );
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
            if (logging( Terse )) log() << L"MonitorFiles - CreateFileW( " << fileName << L", ... ) -> " << handle << endLine;
            fileAccess( fileName, requestedAccessMode( dwDesiredAccess ) );
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
            if (logging( Terse )) log() << L"MonitorFiles - CreateFile2( " << fileName << L", ... ) -> " << handle << endLine;
            fileAccess( fileName, requestedAccessMode( dwDesiredAccess ) );
            return handle;
        }
        typedef BOOL(*TypeDeleteFIleA)(LPCSTR);
        BOOL PatchDeleteFileA( LPCSTR fileName ) {
            TypeDeleteFIleA function = reinterpret_cast<TypeDeleteFIleA>(original( (PatchFunction)PatchDeleteFileA ));
            BOOL deleted = function( fileName );
            if (logging( Terse )) log() << L"MonitorFiles - DeleteFileA( " << fileName << L", ... ) -> " << deleted << endLine;
            fileAccess( fileName, AccessDelete );
            return deleted;
        }
        typedef BOOL(*TypeDeleteFIleW)(LPCWSTR);
        BOOL PatchDeleteFileW( LPCWSTR fileName ) {
            auto function = reinterpret_cast<TypeDeleteFIleW>(original( (PatchFunction)PatchDeleteFileW ));
            BOOL deleted = function( fileName );
            if (logging( Terse )) log() << L"MonitorFiles - DeleteFileW( " << fileName << L", ... ) -> " << deleted << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - CopyFileA( " << existingFileName << L", " << newFileName << L", ... ) -> " << copied << endLine;
            fileAccess( existingFileName, AccessRead );
            auto access = fileAccess( newFileName, AccessWrite );
            getLastWriteTime( access.fileName, access.state.lastWriteTime );
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
            if (logging( Terse )) log() << L"MonitorFiles - CopyFileW( " << existingFileName << L", " << newFileName << L", ... ) -> " << copied << endLine;
            fileAccess( existingFileName, (AccessRead | AccessDelete) );
            auto access = fileAccess( newFileName, AccessWrite );
            getLastWriteTime( access.fileName, access.state.lastWriteTime );
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
            if (logging( Terse )) log() << L"MonitorFiles - CopyFileExA( " << existingFileName << L", " << newFileName << L", ... ) -> " << copied << endLine;
            fileAccess( existingFileName, AccessRead );
            auto access = fileAccess( newFileName, AccessWrite );
            getLastWriteTime( access.fileName, access.state.lastWriteTime );
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
            if (logging( Terse )) log() << L"MonitorFiles - CopyFileExW( " << existingFileName << L", " << newFileName << L", ... ) -> " << copied << endLine;
            fileAccess( existingFileName, AccessRead );
            auto access = fileAccess( newFileName, AccessWrite );
            getLastWriteTime( access.fileName, access.state.lastWriteTime );
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
            if (logging( Terse )) log() << L"MonitorFiles - CopyFile2( " << existingFileName << L", " << newFileName << L", ... ) -> " << copied << endLine;
            fileAccess( existingFileName, AccessRead );
            auto access = fileAccess( newFileName, AccessWrite );
            getLastWriteTime( access.fileName, access.state.lastWriteTime );
            return copied;
        }
        typedef BOOL(*TypeMoveFileA)(LPCSTR,LPCSTR);
        BOOL PatchMoveFileA(
            LPCSTR existingFileName,
            LPCSTR newFileName
        ) {
            auto function = reinterpret_cast<TypeMoveFileA>(original( (PatchFunction)PatchMoveFileA ));
            BOOL moved = function( existingFileName, newFileName );
            if (logging( Terse )) log() << L"MonitorFiles - MoveFileA( " << existingFileName << L", " << newFileName << L", ... ) -> " << moved << endLine;
            fileAccess( existingFileName, AccessDelete );
            auto access = fileAccess( newFileName, AccessWrite );
            getLastWriteTime( access.fileName, access.state.lastWriteTime );
            return moved;
        }       
        typedef BOOL(*TypeMoveFileW)(LPCWSTR,LPCWSTR);
        BOOL PatchMoveFileW(
            LPCWSTR existingFileName,
            LPCWSTR newFileName
        ) {
            auto function = reinterpret_cast<TypeMoveFileW>(original( (PatchFunction)PatchMoveFileW ));
            BOOL moved = function( existingFileName, newFileName );
            if (logging( Terse )) log() << L"MonitorFiles - MoveFileW( " << existingFileName << L", " << newFileName << L", ... ) -> " << moved << endLine;
            fileAccess( existingFileName, AccessDelete );
            auto access = fileAccess( newFileName, AccessWrite );
            getLastWriteTime( access.fileName, access.state.lastWriteTime );
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
            if (logging( Terse )) log() << L"MonitorFiles - MoveFileWithProgressA( " << existingFileName << L", " << newFileName << L", ... ) -> " << moved << endLine;
            fileAccess( existingFileName, AccessDelete );
            auto access = fileAccess( newFileName, AccessWrite );
            getLastWriteTime( access.fileName, access.state.lastWriteTime );
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
            if (logging( Terse )) log() << L"MonitorFiles - MoveFileWithProgressW( " << existingFileName << L", " << newFileName << L", ... ) -> " << moved << endLine;
            fileAccess( existingFileName, AccessDelete );
            auto access = fileAccess( newFileName, AccessWrite );
            getLastWriteTime( access.fileName, access.state.lastWriteTime );
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
            if (logging( Terse )) log() << L"MonitorFiles - FindFirstFileA( " << fileName << L", ... ) -> " << handle << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - FindFirstFileW( " << fileName << L", ... ) -> " << handle << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - FindFirstFileExA( " << fileName << L", ... ) -> " << handle << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - FindFirstFileExW( " << fileName << L", ... ) -> " << handle << endLine;
            fileAccess( fileName, AccessRead );
            return handle;
        }
        typedef DWORD(*TypeGetFileAttributesA)(LPCSTR);
        DWORD PatchGetFileAttributesA(
            LPCSTR fileName
        ) {
            auto function = reinterpret_cast<TypeGetFileAttributesA>(original( (PatchFunction)PatchGetFileAttributesA ));
            DWORD attributes = function( fileName );
            if (logging( Terse )) log() << L"MonitorFiles - GetFileAttributesA( " << fileName << L", ... ) -> " << attributes << endLine;
            fileAccess( fileName, AccessRead );
            return attributes;
        }
        typedef DWORD(*TypeGetFileAttributesW)(LPCWSTR);
        DWORD PatchGetFileAttributesW(
            LPCWSTR fileName
        ) {
            auto function = reinterpret_cast<TypeGetFileAttributesW>(original( (PatchFunction)PatchGetFileAttributesW ));
            DWORD attributes = function( fileName );
            if (logging( Terse )) log() << L"MonitorFiles - GetFileAttributesW( " << fileName << L", ... ) -> " << attributes << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - GetFileAttributesExA( " << fileName << L", ... ) -> " << attributes << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - GetFileAttributesExW( " << fileName << L", ... ) -> " << attributes << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - SetFileAttributesA( " << fileName << L", ... ) -> " << set << endLine;
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
            if (logging( Terse )) log() << L"MonitorFiles - SetFileAttributesW( " << fileName << L", ... ) -> " << set << endLine;
            fileAccess( fileName, AccessWrite );
            return set;
        }

        typedef BOOL(*TypeCloseHandle)(HANDLE);
        BOOL PatchCloseHandle( HANDLE handle ) {
            auto function = reinterpret_cast<TypeCloseHandle>(original( (PatchFunction)PatchCloseHandle ));
            // Record last write time when closing file opened for write
            wstring fileName = fullName( handle );
            auto entry = accessedFiles.find( fileName );
            if ((entry != accessedFiles.end()) && ((accessModes( handle ) & AccessWrite) != 0)) {
                getLastWriteTime( handle, entry->second.lastWriteTime );
                if (logging( Verbose )) log() << L"MonitorFiles - Last write time " << entry->second.lastWriteTime << L" for write access on " << fileName.c_str() << endLine;
            }
            BOOL closed = function( handle );
            if (logging( Terse )) log() << L"MonitorFiles - CloseHandle( " << fileName.c_str() << L", ... ) -> " << closed << endLine;
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

        // Determine access modes to opened file
        FileAccessMode accessModes( HANDLE handle ) {
            FileAccessMode mode( AccessNone );
            uint32_t flags;
            DWORD error = GetLastError();
            DWORD infoError = GetFileInformationByHandleEx( handle, (FILE_INFO_BY_HANDLE_CLASS)8, &flags, sizeof( flags ) );
            if (infoError == ERROR_SUCCESS) {
                if ((FILE_READ_DATA & flags) != 0) mode |= AccessRead;
                if ((FILE_WRITE_DATA & flags) != 0) mode |= AccessWrite;
                if ((FILE_APPEND_DATA & flags) != 0) mode |= AccessWrite;
            }
            SetLastError( error );
            return mode;
        }


        // Convert FileAccessMode value to string
        wstring modeString( FileAccessMode mode ) {
            auto s = wstring( L"" );
            if ((mode & AccessRead) != 0) s += L"Read ";
            if ((mode & AccessWrite) != 0) s += L"Write ";
            if ((mode & AccessDelete) != 0) s += L"Delete ";
            if ((mode & AccessVariable) != 0) s += L"Variable ";
            return s;
        }

        // File name from Windows handle
        wstring fullName( HANDLE handle ) {
            wchar_t fileNameString[ MaxFileName ];
            wstring fileName;
            if ( GetFinalPathNameByHandleW( handle, fileNameString, MaxFileName, 0 ) ) {
                fileName = fileNameString;
                if (fileName.starts_with( L"\\\\?\\" ))  fileName = &fileNameString[ 4 ];
                if (fileName.starts_with( L"\\\\.\\" ))  fileName = &fileNameString[ 4 ];
            }
            return fileName;
        }

        inline wstring widen( const string& fileName ) {
        if (fileName.empty()) return {};
        int len = MultiByteToWideChar( CP_UTF8, 0, &fileName[0], fileName.size(), NULL, 0 );
        wstring wideFileName( len, 0 );
        MultiByteToWideChar( CP_UTF8, 0, &fileName[0], fileName.size(), &wideFileName[0], len );
        return wideFileName;
        }
        // ToDo: Remove use of GetFullPathName as this makes use of of the current directory global
        // and is therefore not thread safe.
        // Expand file name to full path (according to Windows)
        wstring fullName( const char* fileName ) {
            return fullName( widen( string( fileName ) ) );
        }
        wstring fullName( const wchar_t* fileName ) {
            wchar_t filePath[ MaxFileName ];
            wchar_t* fileNameAddress;
            DWORD error = GetLastError();
            DWORD length = GetFullPathNameW( fileName, MaxFileName, filePath, &fileNameAddress );
            SetLastError( error );
            if (length == 0) return wstring( fileName );
            return wstring( filePath );
        }
        wstring fullName( const wstring& fileName ) {
            return fullName( fileName.c_str() );
        }

        // Retrieve last write time on a file (handle)
        // Note that Windows file times have (limited) millisecond resolution
        void getLastWriteTime( HANDLE handle, FileTime& timePoint ) {
            FILETIME fileTime;
            DWORD error = GetLastError();
            if (GetFileTime( handle, nullptr, nullptr, &fileTime )) {
                // Convert Windows FILETIME to C++ chrono::time_point as system clock time
                SYSTEMTIME systemFileTime;
                if (FileTimeToSystemTime( &fileTime, &systemFileTime )) {
                    tm time;
                    time.tm_year = (systemFileTime.wYear - 1900);
                    time.tm_mon = (systemFileTime.wMonth - 1);
                    time.tm_mday = systemFileTime.wDay;
                    time.tm_hour = systemFileTime.wHour;
                    time.tm_min = systemFileTime.wMinute;
                    time.tm_sec = systemFileTime.wSecond;
                    time.tm_isdst = 0;
                    timePoint = chrono::system_clock::from_time_t( mktime( &time ) );
                    timePoint += (systemFileTime.wMilliseconds * chrono::milliseconds(1) );
                }
            }
            SetLastError( error );
        }

        // Get last write time of a (named) file
        void getLastWriteTime( const wstring& fileName, FileTime& timePoint ) {
            wstring fullFileName = fullName( fileName );
            DWORD error = GetLastError();
            auto createFile = reinterpret_cast<TypeCreateFileW>(original( (PatchFunction)PatchCreateFileW ));
            HANDLE handle = createFile( fullFileName.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
            DWORD createError = GetLastError();
            if (createError == ERROR_SUCCESS) {
                getLastWriteTime( handle, timePoint );
                if (logging( Verbose )) log() << L"MonitorFiles - Last write time " << timePoint << L" for access on " << fileName << endLine;
                auto closeFile = reinterpret_cast<TypeCloseHandle>(original( (PatchFunction)PatchCloseHandle ));
                closeFile( handle );
            }
            SetLastError( error );
        }
        void getLastWriteTime( const wstring& fileName ) {
            wstring fullFileName = fullName( fileName.c_str() );
            auto access = accessedFiles.find( fullFileName );
            if (access != accessedFiles.end()) getLastWriteTime( fullFileName, access->second.lastWriteTime );
        }

        // Register file access mode on file (path)
        FileAccess fileAccess( const wstring& fileName, FileAccessMode mode ) {
            auto fullFileName = fullName( fileName );
            if (logging( Terse )) log() << L"MonitorFiles - " << modeString( mode ) << L"access on file " << fullFileName << endLine;
            auto insertion = accessedFiles.insert( pair{ fullFileName, FileAccessState() } );
            FileAccessState& fileAccessData = insertion.first->second;
            if ( insertion.second && (mode == AccessRead) ) {
                // First access on file is read, record last write time...
                getLastWriteTime( insertion.first->first, fileAccessData.lastWriteTime );
            }
            fileAccessData.accessedModes |= mode;
            return { fullFileName, fileAccessData };
        }
        FileAccess fileAccess( const string& fileName, FileAccessMode mode ) {
            return fileAccess( widen( fileName ), mode );
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
    }

    void streamAccessedFiles( wostream& stream ) {
        for ( auto access : accessedFiles ) {
            const wstring& file = access.first;
            const FileAccessState& data = access.second;
            stream << file << " [ " << data.lastWriteTime << " ] " << modeString( data.accessedModes ) << "\n";
        }
    };

} // namespace AccessMonitor