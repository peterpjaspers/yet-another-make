#pragma once

#include "IDirectoryWatcher.h"
#include "Delegates.h"

#include <memory>

namespace YAM
{
	// DirectoryWatcher is a portable (across Windows, Linux, MacOS) implementation
	// of IDirectoryWatcher.
	class __declspec(dllexport) DirectoryWatcher : public IDirectoryWatcher
	{
	public:
		DirectoryWatcher(
			std::filesystem::path const& directory, 
			bool recursive,
			Delegate<void, FileChange const&> const& changeHandler);

	private:
		std::shared_ptr<IDirectoryWatcher> _impl;
	};
}

