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

    std::chrono::time_point<std::chrono::utc_clock> toUtc(std::filesystem::file_time_type ftime) {
        return decltype(ftime)::clock::to_utc(ftime);
    }

    std::chrono::time_point<std::chrono::utc_clock> readLastWriteTime(std::filesystem::path const& path) {
        std::error_code ec;
        return toUtc(std::filesystem::last_write_time(path, ec));
    }
}

namespace YAM
{
    DirectoryWatcherWin32::DirectoryWatcherWin32(
        std::filesystem::path const& directory,
        bool recursive,
        Delegate<void, FileChange const&> const& changeHandler,
        bool suppressSpuriousEvents)
        : IDirectoryWatcher(directory, recursive, changeHandler)
        , _dirHandle(createHandle(directory))
        , _changeBufferSize(32*1024)
        , _changeBuffer(new uint8_t[_changeBufferSize])
        , _suppressSpuriousEvents(suppressSpuriousEvents)
        , _stop(false)
    {
        if (_dirHandle == INVALID_HANDLE_VALUE) throw std::exception("Dir handle creation failed");
        _overlapped.hEvent = CreateEvent(NULL, FALSE, 0, NULL);
        if (_overlapped.hEvent == INVALID_HANDLE_VALUE) throw std::exception("Event handle creation failed");
        if (_suppressSpuriousEvents) {
            fillDirUpdateTimes(_directory, readLastWriteTime(_directory));
        }

        queueReadChangeRequest();
        _watcher = std::make_unique<std::thread>(&DirectoryWatcherWin32::run, this);
    }

    DirectoryWatcherWin32::~DirectoryWatcherWin32() {
        stop();
    }

    void DirectoryWatcherWin32::stop() {
        if (!_stop) {
            _stop = true;
            SetEvent(_overlapped.hEvent); // to wakeup WaitForSingleObject
            _watcher->join();
            CancelIo(_dirHandle);
            CloseHandle(_overlapped.hEvent);
            CloseHandle(_dirHandle);
        }
    }

    void DirectoryWatcherWin32::fillDirUpdateTimes(
        std::filesystem::path const& absPath,
        std::chrono::time_point<std::chrono::utc_clock> const& lwt
    ) {
        if (std::filesystem::is_directory(absPath)) {
            isSpuriousDirModifiedEvent(absPath, lwt);
            for (auto const& entry : std::filesystem::directory_iterator(absPath)) {
                auto flwt = entry.last_write_time();
                fillDirUpdateTimes(absPath / entry.path(), decltype(flwt)::clock::to_utc(flwt));
            }
        }
    }

    bool DirectoryWatcherWin32::isSpuriousDirModifiedEvent(
        std::filesystem::path const& absPath,
        std::chrono::time_point<std::chrono::utc_clock> const& lwt
    ) {
        auto dit = _dirUpdateTimes.find(absPath);
        if (dit != _dirUpdateTimes.end()) {
            if (dit->second == lwt) {
                return true;
            }
        }
        _dirUpdateTimes[absPath] = lwt;
        return false;
    }

    bool DirectoryWatcherWin32::isSpuriousModifiedEvent(
        std::filesystem::path const& absPath,
        std::chrono::time_point<std::chrono::utc_clock>& lwt
    ) {
        if (std::filesystem::is_directory(absPath)) {
            return isSpuriousDirModifiedEvent(absPath, lwt);
        }
        return false;
    }

    void DirectoryWatcherWin32::removeFromDirUpdateTimes(std::filesystem::path const& absPath) {
        auto it = _dirUpdateTimes.find(absPath);
        if (it != _dirUpdateTimes.end()) {
            _dirUpdateTimes.erase(it);
        }
    }

    void DirectoryWatcherWin32::queueReadChangeRequest() {
        BOOL success = ReadDirectoryChangesW(
            _dirHandle, _changeBuffer.get(), _changeBufferSize, TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE,
            NULL, &_overlapped, NULL);
        if (!success) throw std::exception("ReadDirectoryChangesW failed");
    }

    void DirectoryWatcherWin32::run() {
        FileChange rename{ FileChange::Action::None };
        FileChange change{ FileChange::Action::None };
        while (!_stop) {
            DWORD result = WaitForSingleObject(_overlapped.hEvent, INFINITE);
            if (!_stop && result == WAIT_OBJECT_0) {
                DWORD bytesTransferred = 0;
                GetOverlappedResult(_dirHandle, &_overlapped, &bytesTransferred, FALSE);
                if (bytesTransferred == 0) {
                    FileChange overflow;
                    overflow.action = FileChange::Action::Overflow;
                    _changeHandler.Execute(overflow);
                } else {
                    std::size_t offset = 0;
                    PFILE_NOTIFY_INFORMATION info;
                    do
                    {
                        info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(&_changeBuffer.get()[offset]);
                        offset += info->NextEntryOffset;
                        std::wstring fileNameStr(info->FileName, info->FileNameLength / sizeof(wchar_t));
                        std::filesystem::path fileName(fileNameStr);
                        std::filesystem::path absFileName = _directory / fileName;
                        std::error_code ec;
                        change.lastWriteTime = readLastWriteTime(absFileName);
                        if (info->Action == FILE_ACTION_ADDED) {
                            change.action = FileChange::Action::Added;
                            change.fileName = fileName;
                            if (_suppressSpuriousEvents) isSpuriousModifiedEvent(absFileName, change.lastWriteTime);
                        } else if (info->Action == FILE_ACTION_REMOVED) {
                            change.action = FileChange::Action::Removed;
                            change.fileName = fileName;
                            if (_suppressSpuriousEvents) removeFromDirUpdateTimes(absFileName);
                        } else if (info->Action == FILE_ACTION_MODIFIED) {
                            change.action = FileChange::Action::Modified;
                            change.fileName = fileName;
                            if (
                                _suppressSpuriousEvents &&
                                isSpuriousModifiedEvent(absFileName, change.lastWriteTime)
                            ) {
                                change.action = FileChange::Action::None;
                            }
                        } else if (info->Action == FILE_ACTION_RENAMED_OLD_NAME) {
                            rename.action = FileChange::Action::Renamed;
                            rename.oldFileName = fileName;
                            if (_suppressSpuriousEvents) removeFromDirUpdateTimes(_directory / fileName);
                        } else if (info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                            rename.action = FileChange::Action::Renamed;
                            rename.fileName = fileName;
                            if (_suppressSpuriousEvents) isSpuriousModifiedEvent(absFileName, change.lastWriteTime);
                        }
                        if (
                            rename.action == FileChange::Action::Renamed 
                            && !rename.fileName.empty() 
                            && !rename.oldFileName.empty()
                        ) {
                            _changeHandler.Execute(rename);
                            rename.action = FileChange::Action::None;
                            rename.fileName = "";
                            rename.oldFileName = "";
                        } else if (change.action != FileChange::Action::None) {
                            _changeHandler.Execute(change);
                            change.action = FileChange::Action::None;
                        }
                    } while (info->NextEntryOffset != 0);
                }
                queueReadChangeRequest();
            }
        }
    }
}