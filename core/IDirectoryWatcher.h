#pragma once

#include "Delegates.h"
#include <filesystem>
#include <string>

namespace YAM
{
	struct __declspec(dllexport) FileChange {
		// Take care: changes can be reported in various ways.
		// Adding a file F to dir A can be reported in various ways:
		//  - Added A/F
		//  - Modified A
		//  - Added A/F and Modified A
		// Remove a file F from dir A can be reported in various ways:
		//  - Removed A/F
		//  - Modified A
		//  - Removed A/F and Modified A
		// Renaming a file A/F to B/G can be reported in various ways: 
		//	- Removed file A/F + Added file B/G
		//  - Renamed file B/G, oldFile A/F 
		//	- Removed file A/F + modified directory B
		enum Action {
			None = 0,
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

	// A directory watcher monitors a directory tree for changes and informs
	// the application of such changes by execution a delegate to which it
	// passes a FileChange object.
	//
	class __declspec(dllexport) IDirectoryWatcher
	{
	public:
		// Create a watcher that monitors the given directory in a
		// watcher thread that is started/controlled by the watcher.
		// Execute the given delegate when a change is detected.
		// File names in FileChanges are relative to the watched directory.
		// Take care: execution is done in watcher thread context.
		IDirectoryWatcher(
			std::filesystem::path const& directory,
			bool recursive,
			Delegate<void, FileChange const&> const& changeHandler)
			: _directory(directory)
			, _recursive(recursive)
			, _changeHandler(changeHandler)
		{}

		virtual ~IDirectoryWatcher() {}
		std::filesystem::path const& directory() const { return _directory; }
		bool recursive() const { return _recursive; }

	protected:
		std::filesystem::path _directory;
		bool _recursive;
		Delegate<void, FileChange const&> _changeHandler;
	};
}


