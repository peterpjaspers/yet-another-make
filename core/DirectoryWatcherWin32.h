#pragma once

#include "IDirectoryWatcher.h"
#include "Delegates.h"

#include <windows.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>
#include <map>
#include <condition_variable>

namespace YAM
{
    class DirectoriesWatcherWin32;

    class __declspec(dllexport) DirectoryWatcherWin32 : 
        public IDirectoryWatcher,
        public std::enable_shared_from_this<DirectoryWatcherWin32>
    {
    public:
        friend class DirectoriesWatcherWin32;

        // Construct watcher that watches changes in given 'directory'.
        // Call 'changeHandler' when a change is detected.
        // 
        // Modified events for directories can be delivered late, often as
        // side-effect of the first read-access to the directory after its
        // creation. Such spurious events can be suppressed at time-cost of
        // an inital directory tree traversal to retrieve the last-write-time
        // of all directories in the tree and at memory-cost to store all
        // these paths and write times in memory.
        DirectoryWatcherWin32(
            std::filesystem::path const& directory,
            bool recursive,
            Delegate<void, FileChange const&> const& changeHandler,
            bool suppressSpuriousEvents = false);

        ~DirectoryWatcherWin32(); 

        void start() override;
        void stop() override;


    private:
        void queueReadChangeRequest();
        void fillDirUpdateTimes(
            std::filesystem::path const& absPath,
            std::chrono::time_point<std::chrono::utc_clock> const& lwt);
        bool registerSpuriousDirModifiedEvent(
            std::filesystem::path const& absPath,
            std::chrono::time_point<std::chrono::utc_clock> const& lwt);
        bool registerSpuriousModifiedEvent(
            std::filesystem::path const& absPath,
            std::chrono::time_point<std::chrono::utc_clock>& lwt);
        void removeFromDirUpdateTimes(std::filesystem::path const& absPath);

        HANDLE dirHandle() { return _dirHandle; }
        void closeDirHandle();
        OVERLAPPED* overlapped() { return &_overlapped; }
        void processNotifications(DWORD nrBytesReceived);
        void processNotification(PFILE_NOTIFY_INFORMATION info);

        HANDLE _dirHandle;
        DWORD _changeBufferSize; // in nr of DWORDs
        std::unique_ptr<uint8_t> _changeBuffer;
        OVERLAPPED _overlapped;

        bool _suppressSpuriousEvents;
        std::map<std::filesystem::path, std::chrono::time_point<std::chrono::utc_clock>> _dirUpdateTimes; 
        FileChange _rename;
    };
}

