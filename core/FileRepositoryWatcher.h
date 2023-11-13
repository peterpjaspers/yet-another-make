#pragma once

#include "RegexSet.h"
#include "CollapsedFileChanges.h"

#include <memory>
#include <filesystem>
#include <map>

namespace YAM
{
    class Node;
    class ExecutionContext;
    class IDirectoryWatcher;
    class FileRepository;

    // A FileRepositoryWatcher continuously watches a file repository for
    // directory and file changes. 
    //  
    // Changes are queued. The queued changes can be dequeued via the function
    // consumeChanges(). On consumption DirectoryNodes and FileNodes associated 
    // with the changes are marked Dirty. These nodes are looked-up in an 
    // ExecutionContext.
    // 
    // consumeChanges() can be called at any time between builds. At latest it
    // must be called at the start of a build. The build must then sync the 
    // dirty directory and file nodes with the latest file system state by 
    // executing them.
    //
    // During a build generated files will be created/modified and the watched
    // file repository will be notified of these changes. The associated nodes
    // will be marked Dirty at next consumeChanges() call, resulting in 
    // re-executions of these nodes during the next build. Re-execution will 
    // retrieve the file's last-write-time and, only when last-write-time 
    // changed, rehash the file content. Most of these re-executions are not
    // necessary because the last-write-time will only change when the user 
    // tampers with the file. This class reduces the number of unnecessary 
    // re-executions by only marking a generated file node Dirty when its 
    // last-write-time differs from the last-write-time reported in the 
    // FileChange event.
    //
    class __declspec(dllexport) FileRepositoryWatcher
    {
    public:
        // Recursively watch given 'directory' for subdirectory and file 
        // changes. Find directory and file nodes associated with changes in 
        // given 'context->nodes()'.
        FileRepositoryWatcher(
            std::filesystem::path const& directory,
            ExecutionContext* context);

        // Recursively watch given repository for subdirectory and file 
        // changes. Find directory and file nodes associated with changes in 
        // given 'context->nodes()'.
        FileRepositoryWatcher(
            FileRepository* repo,
            ExecutionContext* context);

        std::filesystem::path const& directory();

        // Consume the changes that occurred in the filesystem since the previous
        // consumption by marking directory and file nodes associated with these 
        // changes as Dirty.
        void consumeChanges();

        // Return whether dir/file identified by 'path' has changed since 
        // previous consumeChanges().
        bool hasChanged(std::filesystem::path const& path);

    private:
        void _addChange(FileChange const& change);

        void _handleChange(FileChange const& change);
        void _handleAdd(FileChange const& change);
        void _handleRemove(FileChange const& change);
        void _handleModification(FileChange const& change);
        void _handleOverflow();

        std::shared_ptr<Node> _invalidateNode(
            std::filesystem::path const& path,
            std::chrono::time_point<std::chrono::utc_clock> const& lastWriteTime);
        void _invalidateNodeRecursively(std::filesystem::path const& path);
        void _invalidateNodeRecursively(std::shared_ptr<Node> const& node);

        ExecutionContext* _context;
        FileRepository* _repository;
        CollapsedFileChanges _changes;
        std::shared_ptr<IDirectoryWatcher> _watcher;
    };
}


