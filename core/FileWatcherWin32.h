#pragma once

#include "IFileWatcher.h"
#include "Delegates.h"

#include <windows.h>
#include <atomic>
#include <thread>

namespace YAM
{
	class __declspec(dllexport) FileWatcherWin32 : public IFileWatcher
	{
	public:
		FileWatcherWin32(
			std::filesystem::path const& directory,
			bool recursive,
			Delegate<void, FileChange const&> const& changeHandler);

		~FileWatcherWin32();

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

