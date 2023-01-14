#pragma once

#include "IFileWatcher.h"
#include "Delegates.h"

#include <memory>

namespace YAM
{
	// FileWatcher is a portable (across Windows, Linus, MacOS) implementation
	// of file watching.
	class __declspec(dllexport) FileWatcher : public IFileWatcher
	{
	public:
		FileWatcher(
			std::filesystem::path const& directory, 
			bool recursive,
			Delegate<void, FileChange>& changeHandler);

	private:
		std::shared_ptr<IFileWatcher> _impl;
	};
}

