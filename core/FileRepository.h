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
    class FileExecSpecsNode;


    // A FileRepository is associated with a directory tree. 
    // The directory tree may contain one or more of:
    //      - source file
    //      - build file
    //      - generated file
    // 
    // FileRepository represents the root of the directory tree by a
    // DirectoryNode.
    // 
    // A FileRepository can be configured to be tracked. In a tracked 
    // repository yam will:
    //     - execute the repository root DirectoryNode to mirror the tree in
    //       the buildstate and to find buildfiles.
    //     - register input file dependencies of command and other nodes in 
    //       the buildstate
    //     - watch the directory tree for changes and set the state of file 
    //       and directory nodes associated with these changes to Dirty.
    // If a repository is not tracked then yam will do none of the above.
    //  
    // Watching of a tracked repository is done on request. This allows:
    //     - applications that only read buildstate to not unnecessarily
    //       watch the directory tree
    //     - applications that update the buildstate (e.g. yam itself) to
    //       choose whether to watch or not to watch. The former is needed
    //       to obtain beta-build behavior, the latter will result in 
    //       alpha-build behavior.
    //     - yam to run on platforms for which watching is not (yet) 
    //       implemented.
    // 
    // FileRepository supports the conversion of so-called symbolic paths
    // to/from absolute paths. The format of a symbolic path is
    // @@repoName\relPath where repoName is the name of the repository and 
    // relPath is a path relative to the root directory of the repository.
    // E.g. given a repo with name XYZ and root dir C:\repos\XYZ_root. Then
    // the following paths convert to/from each other:
    //        Symbolic path   <=>    Absolute path
    //     @@XYZ\src\main.cpp <=> C:\repos\XYZ_root\src\main.cpp 
    //
    class __declspec(dllexport) FileRepository : public IPersistable
    {
    public:
        FileRepository(); // needed for deserialization
        FileRepository(
            std::string const& repoName,
            std::filesystem::path const& directory,
            ExecutionContext* context,
            bool tracked);

        virtual ~FileRepository();

        bool tracked() const { return _tracked; }

        // Stop/start watching. Ignored when !tracked().
        // Also ignored when watching is not implemented.
        void stopWatching();
        void startWatching();

        // Return whether directory tree is actually being watched for changes.
        bool watching() const;

        // If watching(): consume the changes that occurred in the repo 
        // directory tree since the previous consumeChanges() call by marking
        // directory and file nodes associated with these changes as Dirty.
        // If !watching(): mark all directory and file nodes Dirty
        void consumeChanges();

        // If watching():
        // Return whether dir/file identified by 'path' has changed since 
        // previous consumeChanges().
        // If !watching(): return true
        bool hasChanged(std::filesystem::path const& path);
        
        // Return whether path starts with @@
        static bool isSymbolicPath(std::filesystem::path const& path);

        // Extract the repository name from path
        // Return empty string when !isSymbolicPath(path)
        static std::string repoNameFromPath(std::filesystem::path const& path);

        // Return @@repoName, e.g. when repoName="main" return @@main
        static std::filesystem::path repoNameToSymbolicPath(std::string const& repoName);

        std::string const& name() const;
        std::filesystem::path const& directory() const;
        std::shared_ptr<DirectoryNode> directoryNode() const;
        std::shared_ptr<FileExecSpecsNode> fileExecSpecsNode() const;

        std::filesystem::path symbolicDirectory() const {
            return repoNameToSymbolicPath(_name);
        }

        // Return whether 'path' is an absolute path or symbolic path in
        // the repository.
        // E.g. if repository directory = C:\a\b and name is XYZ then repo
        //    lexicallyContains("C:\a\b\c\e")
        //    lexicallyContains("C:\a\b")
        //    lexicallyContains("@@XYZ")
        //    lexicallyContains("@@XYZ\c\e")
        //    !lexicallyContains("C:\a")
        //    !lexicallyContains("a\b\c")
        // Note: a lexically contained path need not exist in the file system.
        bool lexicallyContains(std::filesystem::path const& path) const;

        // Return 'absPath' relative to the repo directory.
        // Return empty path when:
        //  !absPath.is_absolute() || !lexicallyContains(absPath) || absPath == directory()
        // Pre: absPath must be in normal form, i.e. not contain . and/or .. components.
        std::filesystem::path relativePathOf(std::filesystem::path const& absPath) const;

        // Return given 'absPath' as symbolicDirectory()/relativePathOf(absPath)
        // Return empty path when !lexicallyContains(absPath).
        // Return symbolicDirectory() when absPath == directory().
        // Pre: absPath.is_absolute();
        std::filesystem::path symbolicPathOf(std::filesystem::path const& absPath) const;

        // Return the absolute path of given symbolic path.
        // Return empty path when !lexicallyContains(symbolicPath).
        std::filesystem::path absolutePathOf(std::filesystem::path const& symbolicPath) const;

        // Recursively remove the directory node from context->nodes().
        // Intended to be used when the repo is removed from the set of
        // known repositories.
        void clear();

        // Inherited from IPersistable
        void modified(bool newValue) override;
        bool modified() const override;
        std::string describeName() const override { return _name; }
        std::string describeType() const override { return "FileRepository"; }

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
        ExecutionContext* _context;
        bool _tracked;
        std::shared_ptr<DirectoryNode> _directoryNode;
        std::shared_ptr<FileExecSpecsNode> _fileExecSpecsNode;
        std::shared_ptr<FileRepositoryWatcher> _watcher;
        bool _modified;
    };
}
