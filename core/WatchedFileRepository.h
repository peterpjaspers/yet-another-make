#pragma once

#include "RegexSet.h"
#include "FileRepository.h"
#include "IDirectoryWatcher.h"

#include <memory>
#include <filesystem>
#include <map>

namespace YAM
{
	class Node;
	class ExecutionContext;

	// A WatchedFileRepository continuously watches the file repository for
	// directory and file changes.  
	// Changes are queued for consumption via the function consumeChanges().
	// On consumption the directory and file nodes associated with the changes
	// are marked Dirty. consumeChanges() can be called at any time between 
	// builds and must be called at the start of a build. The latter to make
	// sure that the build takes all modified directories and files into account.
	// Calling consumeChanges() during a build will cause havoc because the 
	// execution logic in the Node class cannot deal with already executed nodes
	// to become Dirty again.
	// Dirty directory and file nodes can be synced with the fileystem state by
	// executing them. Syncing is not performed by WatchedFileRepository.
	//
	// During a build generated files will be created/modified and the watched
	// file repository will be notified of these changes. Without further
	// measure the associated GeneratedFileNodes would be marked Dirty at next
	// consumeChanges() call, resulting in, often, unnecessary re-executions of
	// these nodes at the next build. This overhead is reduced by only marking
	// the generated file node Dirty when its last-write-time differs from the
	// last-write-time reported in the FileChange event.
	//
	class __declspec(dllexport) WatchedFileRepository : public FileRepository
	{
	public:
		// Recursively watch given 'directory' for subdirectory and file 
		// changes. Find directory and file nodes associated with changes in 
		// given 'context->nodes()'.
		// Note: exclude patterns are not used. Instead failure to find nodes
		// in context is ignored.
		WatchedFileRepository(
			std::string const& repoName,
			std::filesystem::path const& directory,
			// TODO: do not pass exclude patterns here.
			// Instead let each DirectoryNode retrieve exclude patterns from 
			// .yamignore, or if absent, from .gitignore.
			RegexSet const& _excludePatterns,
			ExecutionContext* context);

		// Consume the changes that occurred in the filesystem since the previous
		// consumption by marking directory and file nodes associated with these 
		// changes as Dirty.
		// Only call this function from YAM main thread to ensure that consumption
		// is an atomic action.
		void consumeChanges();

		// Return whether dir/file identified by path has changes since previous
		// consumeChanges().
		bool hasChanged(std::filesystem::path const& path) const;

	protected:
		ExecutionContext* _context;

	private:
		void _addChange(FileChange change);
		void _handleChange(FileChange const& change);
		void _handleAdd(FileChange const& change);
		void _handleRemove(FileChange const& change);
		void _handleModification(FileChange const& change);
		void _handleRename(FileChange const& change);
		void _handleOverflow();
		std::shared_ptr<Node> _invalidateNode(
			std::filesystem::path const& path,
			std::chrono::time_point<std::chrono::utc_clock> const& lastWriteTime);
		void _invalidateNodeRecursively(std::filesystem::path const& path);
		void _invalidateNodeRecursively(std::shared_ptr<Node> const& node);

		std::map<std::filesystem::path, FileChange> _changes;
		std::shared_ptr<IDirectoryWatcher> _watcher;
	};
}


