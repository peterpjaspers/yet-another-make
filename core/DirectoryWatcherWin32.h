#pragma once

#include "IDirectoryWatcher.h"
#include "Delegates.h"

#include <windows.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace YAM
{
	class __declspec(dllexport) DirectoryWatcherWin32 : public IDirectoryWatcher
	{
	public:
		DirectoryWatcherWin32(
			std::filesystem::path const& directory,
			bool recursive,
			Delegate<void, FileChange const&> const& changeHandler);

		~DirectoryWatcherWin32();

	private:
		void queueReadChangeRequest();
		void run(); // runs in _thread

		HANDLE _dirHandle;
		DWORD _changeBufferSize;
		std::unique_ptr<uint8_t> _changeBuffer;
		OVERLAPPED _overlapped;

		std::atomic<bool> _stop;
		std::unique_ptr<std::thread> _watcher;
	};
}

