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
    // FileRepository supports the conversion of so-called symbolic paths
    // to/from absolute paths. A symbolic path is an absolute path in which
    // the file repository directory is replaced by the repository name.
    // A symbolic path is therefore a relative path, the first path component
    // being the repository name.
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

        // Return whether 'path' is a path in the repository.
        // 'path' can be a absolute or symbolic path.
        // E.g. if repository directory = /a/b/ and path = /a/b/c/e then 
        // the repository lexically contains path.
        // E.g. if repository name is 'someRepo' and path is someRepo/a/b
        // then repository lexically contains path. 
        // Note: a lexically contained path need not exist in the file system.
        // Pre: 'absPath' must be without . and .. path components.
        bool lexicallyContains(std::filesystem::path const& path) const;

        // Return 'absPath' relative to the repo directory.
        // Return empty path when !lexicallyContains(absPath).
        // Pre: 'absPath' must be without . and .. path components.
        std::filesystem::path relativePathOf(std::filesystem::path const& absPath) const;

        // Return given 'absPath' as repoName/relativePathOf(absPath).
        // Return empty path when !lexicallyContains(absPath).
        // Pre: 'absPath' must be without . and .. path components.
        std::filesystem::path symbolicPathOf(std::filesystem::path const& absPath) const;

        // Return the absolute path of given symbolic path.
        // Return empty path when symbolicPath.begin() != name().
        std::filesystem::path absolutePathOf(std::filesystem::path const& symbolicPath) const;

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
