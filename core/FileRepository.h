#pragma once

#include "RegexSet.h"
#include "IPersistable.h"

#include <memory>
#include <filesystem>
#include <map>

namespace YAM
{
    class DirectoryNode;
    class Node;
    class ExecutionContext;
    class FileRepositoryWatcher;

    // A FileRepository is associated with a directory tree. 
    // The directory tree may contain:
    //      - subdirectories (recursively)
    //      - source files
    //      - build files
    //      - generated files
    //
    // FileRepository represents the root of the directory tree by a
    // DirectoryNode. This node can be used, by executing it, to mirror (cache)
    // the content of the directory tree in yam's buildstate.
    // 
    // FileRepository continuously watches the directory tree for changes.
    // See consumeChanges().
    // 
    // Yam will register, in its buildstate, input and output dependencies on 
    // files/directories in known FileRepositories. Dependencies on files and
    // directories outside known FileRepositories are silently ignored.
    //
    class __declspec(dllexport) FileRepository : public IPersistable
    {
    public:
        FileRepository(); // needed for deserialization
        FileRepository(
            std::string const& repoName,
            std::filesystem::path const& directory,
            ExecutionContext* context);

        virtual ~FileRepository() {}

        std::string const& name() const;
        std::filesystem::path const& directory() const;
        std::shared_ptr<DirectoryNode> directoryNode() const;

        // Return whether 'absPath' is a path in the repo directory.
        // E.g. if directory = /a/b/ and path = /a/b/c/e then it
        // lexically contains path. This is also true when /a/b/c/e does not
        // exist in the file system.
        bool lexicallyContains(std::filesystem::path const& absPath) const;

        // Return 'absPath' relative to the repo directory.
        // Return empty path when !lexicallyContains(absPath).
        // Pre: 'absPath' must be without . and .. path components.
        std::filesystem::path relativePathOf(std::filesystem::path const& absPath) const;

        // Return given 'absPath' as <repoName>/<path relative to repo directory>.
        // Return empty path when !lexicallyContains(absPath).
        // Pre: 'absPath' must be without . and .. path components.
        std::filesystem::path symbolicPathOf(std::filesystem::path const& absPath) const;

        // Consume the changes that occurred in the repo directory tree since
        // the previous consumeChanges() call by marking directory and file nodes
        // associated with these changes as Dirty.
        void consumeChanges();

        // Return whether dir/file identified by 'path' has changed since 
        // previous consumeChanges().
        bool hasChanged(std::filesystem::path const& path);

        // Recursively remove the directory node from context->nodes().
        // Intended to be used when the repo is removed from the set of
        // known repositories.
        void clear();

        // Inherited from IPersistable
        void modified(bool newValue) override;
        bool modified() const override;

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamer (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        void restore(void* context) override;

    private:
        std::string _name;
        std::filesystem::path _directory;
        ExecutionContext* _context;
        std::shared_ptr<FileRepositoryWatcher> _watcher;
        std::shared_ptr<DirectoryNode> _directoryNode;
        bool _modified;
    };
}
