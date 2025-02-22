#include "DirectoryWatcherWin32.h"

#include <string>
#include <map>
#include <filesystem>
#include <mutex>
#include <thread>
#include <memory>

namespace
{
    using namespace YAM;

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
    class DirectoriesWatcherWin32
    {
    private:
        // completionKey->watcher, where completionKey==watcher.get()
        std::map<ULONG_PTR, std::shared_ptr<DirectoryWatcherWin32>> _watchers;
        std::map<ULONG_PTR, std::shared_ptr<DirectoryWatcherWin32>> _removedWatchers;
        std::mutex _watchersMutex;
        HANDLE _iocp; // The i/o completion port
        std::thread _iocpReader;

        void checkDuplicate(std::shared_ptr<DirectoryWatcherWin32> const& watcher) {
            for (auto const& pair : _watchers) {
                if (
                    (pair.second == watcher)
                    || (pair.second->directory() == watcher->directory())
                ) {
                    throw std::runtime_error("attempt to add duplicate watcher");
                }
            }
        }

        void processNotification(DWORD nBytes, ULONG_PTR completionKey) {
            DirectoryWatcherWin32* watcher = reinterpret_cast<DirectoryWatcherWin32*>(completionKey);
            std::lock_guard<std::mutex> lock(_watchersMutex); 
            if (_removedWatchers.contains(completionKey)) {
                if (nBytes == 0) {
                    // Buffer overflow or closing notification of a removed watcher.
                    _removedWatchers.erase(completionKey);
                } else {
                    // ignore notification
                }
            } else if (_watchers.contains(completionKey)) {
                watcher->processNotifications(nBytes);
            } else {
                throw std::runtime_error("Logic error");
            }
        }

    public:
        DirectoriesWatcherWin32()
        {
            _iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
            if (_iocp == NULL) {
                std::error_code ec(GetLastError(), std::system_category());
                throw std::system_error(ec, "DirectoriesWatcher failed to create i/o completion port");
            }
            _iocpReader = std::thread([this]() { iocpReader(); });
        }

        ~DirectoriesWatcherWin32() {
            if (_iocp != INVALID_HANDLE_VALUE) {
                {
                    std::lock_guard<std::mutex> lock(_watchersMutex);
                    for (auto& pair : _watchers) pair.second->stop();
                    // send notification that will stop the iocp reader thread
                    PostQueuedCompletionStatus(_iocp, 0, reinterpret_cast<ULONG_PTR>(this), nullptr);
                }
                _iocpReader.join();
                CloseHandle(_iocp);
                _iocp = INVALID_HANDLE_VALUE;
            }
        }

        void add(std::shared_ptr<DirectoryWatcherWin32> const& watcher) {
            std::lock_guard<std::mutex> lock(_watchersMutex);
            checkDuplicate(watcher);
            ULONG_PTR completionKey = reinterpret_cast<ULONG_PTR>(watcher.get());
            _watchers.insert({ completionKey, watcher });
            if (!CreateIoCompletionPort(watcher->dirHandle(), _iocp, completionKey, 1)) {
                std::error_code ec(GetLastError(), std::system_category());
                throw std::system_error(ec, "Failed to add directory handle to i/o completion port");
            }
        }

        void remove(std::shared_ptr<DirectoryWatcherWin32> const& watcher) {
            // Before a watcher removes itself it will close its directory
            // handle. This will result in ReadDirectoryChanges to send a 
            // closing notification. The notification buffer of the removed
            // watcher must therefore remain allocated until this closing 
            // notification has been received. To prevent the watcher from
            // going out-of-scope (and being destructed), the watcher is
            // kept in _removedWatchers until its closing notification is
            // received.
            std::lock_guard<std::mutex> lock(_watchersMutex);
            ULONG_PTR completionKey = reinterpret_cast<ULONG_PTR>(watcher.get());
            auto it = _watchers.find(completionKey);
            if (it == _watchers.end()) throw std::runtime_error("Unknown watcher");
            auto result = _removedWatchers.insert({ completionKey, watcher });
            if (!result.second) throw std::runtime_error("Failed to remove watcher");
            _watchers.erase(it);
            watcher->closeDirHandle();
        }

        void iocpReader() {
            DWORD nBytes = 0;
            OVERLAPPED* ov = nullptr;
            ULONG_PTR completionKey = 0;
            ULONG_PTR stopKey = reinterpret_cast<ULONG_PTR>(this);
            BOOL success = true;
            bool stopped = false;
            while (!stopped && success) {
                success = GetQueuedCompletionStatus(_iocp, &nBytes, &completionKey, &ov, INFINITE);
                if (success) {
                    if (completionKey == stopKey) {
                        if (!_watchers.empty() || !_removedWatchers.empty()) {
                            throw std::runtime_error("Logic error");
                        }
                        stopped = true;
                    } else {
                        processNotification(nBytes, completionKey);
                    }
                } else {
                    std::error_code ec(GetLastError(), std::system_category());
                    throw std::system_error(ec, "GetQueuedCompletionStatus failed");
                }
            }
        }
    };
}

namespace
{
    DirectoriesWatcherWin32 watcher;
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
        , _changeBufferSize(32*1024/sizeof(DWORD))
        // allocate DWORDs to get a DWORD-aligned buffer
        , _changeBuffer(reinterpret_cast<uint8_t*>(new DWORD[_changeBufferSize]))
        , _suppressSpuriousEvents(suppressSpuriousEvents)
        , _rename {FileChange::Action::None }
    {
        if (_dirHandle == INVALID_HANDLE_VALUE) throw std::exception("Dir handle creation failed");
        _overlapped.hEvent = CreateEvent(NULL, FALSE, 0, NULL);
        if (_overlapped.hEvent == INVALID_HANDLE_VALUE) throw std::exception("Event handle creation failed");
        if (_suppressSpuriousEvents) {
            fillDirUpdateTimes(_directory, readLastWriteTime(_directory));
        }
        queueReadChangeRequest();
    }

    DirectoryWatcherWin32::~DirectoryWatcherWin32() {
        stop();
    }


    void DirectoryWatcherWin32::start() {
        if (_dirHandle != INVALID_HANDLE_VALUE) {
            watcher.add(shared_from_this());
        }
    }

    void DirectoryWatcherWin32::stop() {
        if (_dirHandle != INVALID_HANDLE_VALUE) {
            watcher.remove(shared_from_this());
        }
    }

    void DirectoryWatcherWin32::closeDirHandle() {
        if (_dirHandle != INVALID_HANDLE_VALUE) {
            auto handle = _dirHandle;
            _dirHandle = INVALID_HANDLE_VALUE;
            CloseHandle(handle);
        }
    }

    void DirectoryWatcherWin32::fillDirUpdateTimes(
        std::filesystem::path const& absPath,
        std::chrono::time_point<std::chrono::utc_clock> const& lwt
    ) {
        if (std::filesystem::is_directory(absPath)) {
            registerSpuriousDirModifiedEvent(absPath, lwt);
            for (auto const& entry : std::filesystem::directory_iterator(absPath)) {
                auto flwt = entry.last_write_time();
                fillDirUpdateTimes(absPath / entry.path(), decltype(flwt)::clock::to_utc(flwt));
            }
        }
    }

    // Return whether event is spurious
    bool DirectoryWatcherWin32::registerSpuriousDirModifiedEvent(
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

    // Return whether event is spurious
    bool DirectoryWatcherWin32::registerSpuriousModifiedEvent(
        std::filesystem::path const& absPath,
        std::chrono::time_point<std::chrono::utc_clock>& lwt
    ) {
        if (std::filesystem::is_directory(absPath)) {
            return registerSpuriousDirModifiedEvent(absPath, lwt);
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
        if (_dirHandle != INVALID_HANDLE_VALUE) {
            BOOL success = ReadDirectoryChangesW(
                _dirHandle, _changeBuffer.get(), _changeBufferSize * sizeof(DWORD), TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                NULL, &_overlapped, NULL);
            if (!success) {
                std::error_code ec(GetLastError(), std::system_category());
                throw std::system_error(ec, "ReadDirectoryChangesW failed");
            }
        }
    }

    void DirectoryWatcherWin32::processNotifications(DWORD nrBytesReceived) {
        if (nrBytesReceived == 0) {
            FileChange overflow;
            overflow.action = FileChange::Action::Overflow;
            _changeHandler.Execute(overflow);
        } else {
            std::size_t offset = 0;
            PFILE_NOTIFY_INFORMATION info;
            do {
                info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(&_changeBuffer.get()[offset]);
                processNotification(info);
                offset += info->NextEntryOffset;
            } while (info->NextEntryOffset != 0);
        }
        queueReadChangeRequest();
    }

    void DirectoryWatcherWin32::processNotification(PFILE_NOTIFY_INFORMATION info)
    {
        std::wstring fileNameStr(info->FileName, info->FileNameLength / sizeof(wchar_t));
        std::filesystem::path fileName(_directory / fileNameStr);
        std::error_code ec;
        std::filesystem::path absFileName = std::filesystem::canonical(fileName, ec);
        if (ec) absFileName = fileName;

        YAM::FileChange change{ FileChange::Action::None };
        change.lastWriteTime = readLastWriteTime(absFileName);
        if (info->Action == FILE_ACTION_ADDED) {
            change.action = FileChange::Action::Added;
            change.fileName = absFileName;
            if (_suppressSpuriousEvents) registerSpuriousModifiedEvent(absFileName, change.lastWriteTime);
        } else if (info->Action == FILE_ACTION_REMOVED) {
            change.action = FileChange::Action::Removed;
            change.fileName = absFileName;
            if (_suppressSpuriousEvents) removeFromDirUpdateTimes(absFileName);
        } else if (info->Action == FILE_ACTION_MODIFIED) {
            change.action = FileChange::Action::Modified;
            change.fileName = absFileName;
            if (
                _suppressSpuriousEvents &&
                registerSpuriousModifiedEvent(absFileName, change.lastWriteTime)
            ) {
                change.action = FileChange::Action::None;
            }
        } else if (info->Action == FILE_ACTION_RENAMED_OLD_NAME) {
            _rename.action = FileChange::Action::Renamed;
            _rename.oldFileName = absFileName;
            if (_suppressSpuriousEvents) removeFromDirUpdateTimes(absFileName);
        } else if (info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
            _rename.action = FileChange::Action::Renamed;
            _rename.fileName = absFileName;
            if (_suppressSpuriousEvents) registerSpuriousModifiedEvent(absFileName, change.lastWriteTime);
        }
        if (
            _rename.action == FileChange::Action::Renamed
            && !_rename.fileName.empty()
            && !_rename.oldFileName.empty()
        ) {
            _changeHandler.Execute(_rename);
            _rename.action = FileChange::Action::None;
            _rename.fileName = "";
            _rename.oldFileName = "";
        } else if (change.action != FileChange::Action::None) {
            _changeHandler.Execute(change);
            change.action = FileChange::Action::None;
        }
    }
}