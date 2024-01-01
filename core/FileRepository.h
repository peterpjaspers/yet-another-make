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
    // to/from absolute paths. The format of a symbolic path is
    // <repoName>\relPath where repoName is the name of the repository and 
    // relPath is a path relative to the root directory of the repository.
    // E.g. given a repo with name XYZ and root dir C:\repos\XYZ_root. Then
    // the following paths convert to/from each other:
    //        Symbolic path   <=>    Absolute path
    //     <XYZ>\src\main.cpp <=> C:\repos\XYZ_root\src\main.cpp 
    //
    class __declspec(dllexport) FileRepository : public IPersistable
    {
    public:
        FileRepository(); // needed for deserialization
        FileRepository(
            std::string const& repoName,
            std::filesystem::path const& directory,
            ExecutionContext* context);

        virtual ~FileRepository();
        
        static bool isSymbolicPath(std::filesystem::path const& path);
        // Return empty string when !isSymbolicPath(path)
        static std::string repoNameFromPath(std::filesystem::path const& path);
        // Return <repoName>, e.g. when repoName="main" return "<main>"
        static std::filesystem::path repoNameToSymbolicPath(std::string const& repoName);

        std::string const& name() const;
        std::filesystem::path const& directory() const;
        std::shared_ptr<DirectoryNode> directoryNode() const;

        // Return repositoryNameOf(name())
        std::filesystem::path const& symbolicDirectory() const {
            return _symbolicDirectory;
        }

        // Return whether 'path' is a path in the repository.
        // 'path' can be an absolute path or a symbolic path.
        // E.g. if repository directory = /a/b and path = /a/b/c/e then 
        // the repository lexically contains path.
        // E.g. if repository directory = /a/b and path = /a/b then 
        // the repository lexically contains path.
        // E.g. if repository name is 'someRepo' and path is <someRepo>/a/b
        // then repository lexically contains path. 
        // Note: a lexically contained path need not exist in the file system.
        bool lexicallyContains(std::filesystem::path const& path) const;

        // Return 'absPath' relative to the repo directory.
        // Return empty path when !lexicallyContains(absPath) or when
        // absPath == directory().
        std::filesystem::path relativePathOf(std::filesystem::path const& absPath) const;

        // Return given 'absPath' as <repoName>/absPath.relative_path().
        // Return empty path when !lexicallyContains(absPath).
        // Return name() when absPath == directory().
        // Pre: absPath.is_absolute();
        std::filesystem::path symbolicPathOf(std::filesystem::path const& absPath) const;

        // Return the absolute path of given symbolic path.
        // Return empty path when !lexicallyContains(symbolicPath).
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
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private:
        std::string _name;
        std::filesystem::path _directory;
        std::filesystem::path _symbolicDirectory;
        ExecutionContext* _context;
        std::shared_ptr<FileRepositoryWatcher> _watcher;
        std::shared_ptr<DirectoryNode> _directoryNode;
        bool _modified;
    };
}
