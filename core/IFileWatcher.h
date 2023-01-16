#pragma once

#include "Delegates.h"
#include <filesystem>
#include <string>

namespace YAM
{
	struct __declspec(dllexport) FileChange {
		// Renaming a file A/F to B/G can be notified in various ways: 
		//	- Removed file A/F + Added file B/G
		//  - Renamed file B/G, oldFile A/F 
		//	- Removed file A/F + modified directory B
		enum Action {
			Added = 1,    // file/dir is created
			Removed = 2,  // file/dir is removed
			Modified = 3, // file/dir is modified
			Renamed = 4,  // file/dir is renamed
			Overflow = 5  // lost track of changes due to buffer overflow
		};
		Action action;
		std::filesystem::path fileName;
		std::filesystem::path oldFileName; // only applicable when Renamed
	};

	// A file watcher detects changes to the files and sub-directories in
	// a directory. It informs the application of such changes by execution
	// a delegate to which it passes a FileChange object.
	//
	class __declspec(dllexport) IFileWatcher
	{
	public:
		// Create a watcher that monitors the given directory in a
		// watcher thread that is started/controlled by the watcher.
		// Execute the given delegate when a change is detected.
		// File names in FileChanges are relative to the watched directory.
		// Take care: execution is done in watcher thread context.
		IFileWatcher(
			std::filesystem::path const& directory,
			bool recursive,
			Delegate<void, FileChange const&> const& changeHandler)
			: _directory(directory)
			, _recursive(recursive)
			, _changeHandler(changeHandler)
		{}

		virtual ~IFileWatcher() {}
		std::filesystem::path const& directory() const { return _directory; }
		bool recursive() const { return _recursive; }

	protected:
		std::filesystem::path _directory;
		bool _recursive;
		Delegate<void, FileChange const&> _changeHandler;
	};
}


