#include <windows.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <memory>

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

    class DirectoryWatcher
    {
    private:
        std::filesystem::path _rootDir;

        HANDLE _dirHandle;
        DWORD _bufferSizeInBytes;
        std::unique_ptr<DWORD> _buffer;
        OVERLAPPED _overlapped;

        std::mutex _mutex;
        std::condition_variable _cond;
        bool _running;
        std::atomic<bool> _stop;
        std::unique_ptr<std::thread> _watcher;

    public:
        DirectoryWatcher(std::filesystem::path const& directory)
            : _rootDir(directory)
            , _dirHandle(createHandle(_rootDir))
            , _bufferSizeInBytes(32 * 1024)
            , _buffer(new DWORD[_bufferSizeInBytes / sizeof(DWORD)])
            , _running(false)
            , _stop(false)
        {
            if (_dirHandle == INVALID_HANDLE_VALUE) throw std::runtime_error("dir handle creation failed");
            memset(&_overlapped, 0, sizeof(OVERLAPPED));
            _overlapped.hEvent = CreateEvent(NULL, FALSE, 0, NULL);
            if (_overlapped.hEvent == INVALID_HANDLE_VALUE) throw std::runtime_error("event handle creation failed");
            _watcher = std::make_unique<std::thread>(&DirectoryWatcher::run, this);

            std::unique_lock<std::mutex> lock(_mutex);
            while (!_running) _cond.wait(lock);
        }

        ~DirectoryWatcher() {
            _stop = true;
            SetEvent(_overlapped.hEvent); // to wakeup WaitForSingleObject
            _watcher->join();
            CancelIo(_dirHandle);
            CloseHandle(_overlapped.hEvent);
            CloseHandle(_dirHandle);
        }

    private:
        void queueReadChangeRequest() {
            BOOL success = ReadDirectoryChangesW(
                _dirHandle, _buffer.get(), _bufferSizeInBytes, TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                NULL, &_overlapped, NULL);
            if (!success) throw std::runtime_error("ReadDirectoryChangesW failed");
        }

        void run() {
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
                        std::cout << "Overflow" << std::endl;
                    } else {
                        std::size_t offset = 0;
                        PFILE_NOTIFY_INFORMATION info;
                        do
                        {
                            info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(&_buffer.get()[offset]);
                            offset += info->NextEntryOffset;
                            std::wstring fileNameStr(info->FileName, info->FileNameLength / sizeof(wchar_t));
                            std::filesystem::path fileName(_rootDir / fileNameStr);
                            if (info->Action == FILE_ACTION_ADDED) {
                                std::cout << "Added " << fileName << std::endl;
                            } else if (info->Action == FILE_ACTION_REMOVED) {
                                std::cout << "Removed " << fileName << std::endl;
                            } else if (info->Action == FILE_ACTION_MODIFIED) {
                                std::cout << "Modified " << fileName << std::endl;
                            } else if (info->Action == FILE_ACTION_RENAMED_OLD_NAME) {
                                std::cout << "Renamed old name " << fileName << std::endl;
                            } else if (info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                                std::cout << "Renamed new name " << fileName << std::endl;
                            }
                        } while (info->NextEntryOffset != 0);
                    }
                    queueReadChangeRequest();
                }
            }
        }
    };

    void iterateDirectory(std::filesystem::path const& dir)
    {
        for (auto const& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_directory()) {
                std::filesystem::path s3(dir / entry.path());
                for (auto const& subentry : std::filesystem::directory_iterator(dir)) {
                }
            }
        }
    }
}

int main() {
    std::string tmpDir(std::tmpnam(nullptr));
    std::filesystem::path rootDir(std::string(tmpDir + "_spuriousEvents"));
    std::filesystem::path subDir0(rootDir / "subDir0");
    std::filesystem::path subDir00(subDir0 / "subDir00");
    std::filesystem::create_directory(rootDir);
    std::filesystem::create_directory(subDir0);
    std::filesystem::create_directory(subDir00);
    std::cout << "Created directory " << rootDir.string() << std::endl;
    std::cout << "Created directory " << subDir0.string() << std::endl;
    std::cout << "Created directory " << subDir00.string() << std::endl;

    DirectoryWatcher watcher(rootDir);
    std::cout << std::endl << "Started watching directory tree " << rootDir.string() << std::endl;

    std::cout << std::endl << "Unexpected event during first directory iteration" << std::endl;
    iterateDirectory(subDir0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << std::endl << std::endl;
    std::cout << "No unexpected event during subsequent directory iterations" << std::endl;
    iterateDirectory(subDir0);
    iterateDirectory(subDir0);

    char input = '\0';
    while (input == '\0') std::cin >> input;
}
