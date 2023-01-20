#include "DirectoryWatcherWin32.h"

#include <string>

namespace
{
    HANDLE createHandle(std::filesystem::path const& directory) {
        std::wstring path = directory.wstring();
        LPCWSTR dirPath = static_cast<LPCWSTR>(path.c_str());
        HANDLE handle = CreateFileW(
            dirPath,
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL);
        return handle;
    }
}

namespace YAM
{
    DirectoryWatcherWin32::DirectoryWatcherWin32(
        std::filesystem::path const& directory,
        bool recursive,
        Delegate<void, FileChange const&> const& changeHandler)
        : IDirectoryWatcher(directory, recursive, changeHandler)
        , _dirHandle(createHandle(directory))
        , _changeBufferSize(32*1024)
        , _changeBuffer(new DWORD[_changeBufferSize / sizeof(DWORD)])
        , _running(false)
        , _stop(false)
    {
        _overlapped.hEvent = CreateEvent(NULL, FALSE, 0, NULL);
        assert(_dirHandle != INVALID_HANDLE_VALUE);
        assert(_overlapped.hEvent != INVALID_HANDLE_VALUE);
        _watcher = std::make_unique<std::thread>(&DirectoryWatcherWin32::run, this);

        std::unique_lock<std::mutex> lock(_mutex);
        while (!_running) _cond.wait(lock);
    }

    DirectoryWatcherWin32::~DirectoryWatcherWin32() {
        _stop = true;
        SetEvent(_overlapped.hEvent); // to wakeup WaitForSingleObject
        _watcher->join();
        CancelIo(_dirHandle);
        CloseHandle(_overlapped.hEvent);
        CloseHandle(_dirHandle);
    }

    void DirectoryWatcherWin32::queueReadChangeRequest() {
        BOOL success = ReadDirectoryChangesW(
            _dirHandle, _changeBuffer.get(), _changeBufferSize, TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE,
            NULL, &_overlapped, NULL);
        assert(success);
    }

    void DirectoryWatcherWin32::run() {
        const DWORD waitTimeoutInMs = 100;
        queueReadChangeRequest();
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _running = true;
            _cond.notify_one();
        }
        while (!_stop) {
            DWORD result = WaitForSingleObject(_overlapped.hEvent, INFINITE);
            if (!_stop && result == WAIT_OBJECT_0) {
                DWORD bytesTransferred;
                GetOverlappedResult(_dirHandle, &_overlapped, &bytesTransferred, FALSE);
                if (bytesTransferred == 0) {
                    FileChange overflow;
                    overflow.action = FileChange::Action::Overflow;
                    _changeHandler.Execute(overflow);
                } else {
                    std::size_t offset = 0;
                    PFILE_NOTIFY_INFORMATION info;
                    FileChange rename;
                    rename.action = FileChange::Action::Renamed;
                    do
                    {
                        FileChange change;
                        info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(&_changeBuffer.get()[offset]);
                        offset += info->NextEntryOffset;
                        std::wstring fileNameStr(info->FileName, info->FileNameLength / sizeof(wchar_t));
                        std::filesystem::path fileName(fileNameStr);
                        if (info->Action == FILE_ACTION_ADDED) {
                            change.action = FileChange::Action::Added;
                            change.fileName = fileName;
                        } else if (info->Action == FILE_ACTION_REMOVED) {
                            change.action = FileChange::Action::Removed;
                            change.fileName = fileName;
                        } else if (info->Action == FILE_ACTION_MODIFIED) {
                            change.action = FileChange::Action::Modified;
                            change.fileName = fileName;
                        } else if (info->Action == FILE_ACTION_RENAMED_OLD_NAME) {
                            rename.oldFileName = fileName;
                        } else if (info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                            rename.fileName = fileName;
                        }
                        if (!rename.fileName.empty() && !rename.oldFileName.empty()) {
                            _changeHandler.Execute(rename);
                            rename.fileName = "";
                            rename.oldFileName = "";
                        } else {
                            _changeHandler.Execute(change);
                        }
                    } while (info->NextEntryOffset != 0);
                }
                queueReadChangeRequest();
            }
        }
    }
}