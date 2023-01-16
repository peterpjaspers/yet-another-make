#pragma once

#include "IFileWatcher.h"
#include "Delegates.h"

#include <memory>

namespace YAM
{
	// FileWatcher is a portable (across Windows, Linux, MacOS) implementation
	// of IFileWatcher.
	class __declspec(dllexport) FileWatcher : public IFileWatcher
	{
	public:
		FileWatcher(
			std::filesystem::path const& directory, 
			bool recursive,
			Delegate<void, FileChange const&> const& changeHandler);

	private:
		std::shared_ptr<IFileWatcher> _impl;
	};
}

