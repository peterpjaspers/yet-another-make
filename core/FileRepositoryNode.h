#pragma once

#include "RegexSet.h"
#include "Node.h"

#include <memory>
#include <filesystem>
#include <map>

namespace YAM
{
    class DirectoryNode;
    class ExecutionContext;
    class FileRepositoryWatcher;
    class FileExecSpecsNode;


    // A FileRepository is associated with a directory tree. The tree contains
    // one or more of:
    //      - source file
    //      - build file
    //      - generated file
    // 
    // FileRepository represents the directory tree by a DirectoryNode. 
    // Executing this node mirrors the directory tree in yam's buildstate.
    // 
    // Terms:
    // Home repository
    //      The directory tree in which the user starts a build.
    // Child repository
    //      The home repository may depend on the content of other, 
    //      so-called child, repositories, recursively. The repository 
    //      dependency graph is a a-cyclic directed graph.
    //      See class FileRepositoriesNode.
    // 
    // The FileRepository type defines how yam will use a child repository:
    //    - Local
    //      Yam will treat the repository as if it was a directory in the
    //      home repository: yam will mirror the files/directories, execute,
    //      parse and compile buildfiles, execute the resulting command nodes 
    //      and register detected input file dependencies in the buildstate 
    //      of the home repo.
    //      Advantages:
    //          - subject to buildscope
    //          - buildstate allows dependency analysis across repo boundary.
    //      Disadvantagess:
    //          - possible interference between builds started in child and 
    //            home repos.
    //            A build in a child repo may write generated files previously 
    //            build in the home repo. A subsequent build in the home repo 
    //            will consider these files to be out-dated and rebuild them.
    //    - Foreign (optionally, discuss with Phil)
    //      Before yam builds the home repository it submits build requests to
    //      the yamServers for Coupled child repositories. Yam will register 
    //      detected input file dependencies in the buildstate of the home repo
    //      Optionally: only if yam detected changes in these repos (tracking).
    //      Note: the repository graph defines the build order of Coupled repos.
    //      Advantages:
    //          - no interference between builds in home and Coupled repos.
    //          - Possible to build child with other build tool.
    //      Disadvantages:
    //          - Coupled repo is build with all-scope.
    //          - Limited dependency analysis across repo boundaries.
    //            E.g. one can see that a executable in home repo depends on a
    //            lib in child repo, that a cpp file in home depends on header
    //            files in child. The home buildstate does not contain the
    //            dependencies of generated files in child. For that one has to
    //            inspect the buildstate in the child repo buildstate.
    //          - Always the overhead of up-to-date analysis of the child repos.
    //    - Track
    //      Yam will not build the child repo. Yam will register detected input 
    //      file dependencies in the buildstate of the home repo.
    //    - Ignore
    //      Yam will not build the child repo and (silently) ignores detected
    //      input file dependencies. 
    //      Usage example: ignore C:\windows\Program Files
    // 
    // A FileRepository can be watched. A watched repository subscribes at the
    // filesystem to be notified of changes in the repository directory tree. 
    // The file/directory nodes associated with these changes are marked Dirty.
    // 
    // Watching impacts the time complexity of an increment build:
    //     - watching: O(N), N being the number of changed files/ directories.
    //       A build only scans Dirty files/directories to find the changed ones.
    //     - not watching: O(N), N being the number of all files/directories in
    //       the repository. A build must scan all files/directories to find the
    //       changed ones.
    //  
    // Watching a repository is done on demand. This allows applications that 
    // only read the buildstate to not unnecessarily watch the directory tree.
    // The yam build application watches the home, Integrated and Coupled (??)
    // child repositories.
    // 
    // FileRepository supports the conversion of absolute paths to/from
    // so-called symbolic paths. The format of a symbolic path is
    // @@repoName\relPath where repoName is the name of the repository and 
    // relPath is a path relative to the root directory of the repository.
    // E.g. given a repo with name XYZ and root dir C:\repos\XYZ_root. Then
    // the following paths convert to/from each other:
    //        Symbolic path   <=>    Absolute path
    //     @@XYZ\src\main.cpp <=> C:\repos\XYZ_root\src\main.cpp 
    //
    // 
    // --Repository name and version conflicts. 
    // Consider the following repository dependency graph:
    // 
    //                 H(/P)
    //                  |
    //                  v
    //                 A(/Q)
    //                  |
    //                  v
    //                 B(/R)
    //                  | 
    //                  v  
    //                 A(/S)   
    // 
    // H(/P) is the home repository, i.e. the repo in which builds are done.
    // This repository is stored in directory /P.
    // Repository H depends on repo A in directory /Q.
    // Repository A depends on repo B in directory /R.
    // Repository B depends on repo A in directory /S.
    // Assume that /Q and /S contain different repositories, i.e. not two
    // different versions of repo A.
    // Assume that all repos are of type Integrated.
    // Assume that the graph is constructed from the repository configurations
    // stored in directories /P, /Q, /R and /S. 
    // 
    // A build in H(/P) will mirror directory trees /P, /Q, /R and /S.
    // The names (symbolic paths) of the file nodes associated with files
    // /Q/F and /S/G would then be @@A/F and @@A/G respectively.
    // Problem: converting these node names to absolute paths is ambiguous.
    // This can be fixed by renaming in the repo configuration of H(/P)
    // A(/Q) to e.g. C(/Q) and replacing all references to @A in buildfiles in 
    // H(/P) by references to @@C. 
    // Buildfiles in H(/P) reference @@C/F which is correctly mapped to /Q/F
    // Buildfiles in B(/R) reference @@A/G which is correctly mapped to /S/G.
    // The problem could also be fixed by renaming A(/S) to C(/S) in the 
    // repository configuration of B(/R). That requires all references 
    // to @@A in buildfiles in B(/R) to be replaced by references to @C.
    // 
    // --Repository version conflicts.
    // Consider the following dependency graph:
    // 
    //           +---- H(/P) ----+
    //           |               |
    //           v               v
    //          B(/Q)           C(/R)
    //           |               |    
    //           v               v
    //          D(/S)           D(/T) 
    // 
    // D references the same repository but directories /S and /T contain
    // different versions of D. As discussed above yam cannot deal with
    // duplicate names in the dependency graph. 
    // The problem can be fixed by e.g. changing the child repository
    // configuration in B(/Q) to reference the same version a C(/R). 
    // But doing this impacts other repos that reference B(/Q). This is an 
    // undesired side-effect. 
    // 
    // Repository version conflicts can be fixed by not having repo H(/P) 
    // reconstruct the dependency graph from the repository configurations 
    // in its child repositories but instead by specifying the entire graph 
    // in H(/P). This gives H(/P) full freedom to choose the versions of its 
    // child repositories. It also gives H(/P) freedom to choose the child
    // repository names. But this comes at the cost of having to modify the
    // buildfiles in the renamed repositories.
    //  
    // --Relation with build cache.
    // The rationale behind using symbolic path names as file node names is in
    // the effective use of the build cache. In yam a build cache is associated
    // with one repository. The build cache for some repo B stores the files 
    // generated by builds of different versions of repo B.
    // When a user wants to build a generated file G in some verion of repo B 
    // stored in directory /Q then yam queries the build cache for @@B/G. This
    // query returns the list of cached versions of G. Each version of G 
    // specifies the symbolic paths of the detected input files and the input 
    // hash, i.e. the hash of the hashes of the input files and build script.
    // Versions of G differ in their input hash.
    // The requesting build computes for each version of G the input hash in
    // in its version of B. If one of these hashes matches the input hash of
    // of a cached version of G then the build reuses the generated file
    // cached for the version of G.
    // Without symbolic paths the requesting build would query the cache for 
    // path /Q/G. This will only return versions of G that were built in /Q.
    // Hence for the cache to be effective all past and future builds of 
    // (versions of) B must be executed in /Q. This is highly impractical.
    // 
    // --Recommendation for optimal use of multi-repo builds and build cache
    //     - Use unique repository names
    //     - Avoid renaming of repositories
    //       Changing a repository name from A to B will delete the build cache
    //       for A.
    // 
    // --Repository version
    // There is currently no support to specify the version of a repository.
    // This can easily be added. Cloning/checking-out the repository versions
    // cannot be supported by yam as it must be version mgt agnostic.
    // 
    // --Concern
    // Setting-up multi-repo development is complex. Using git submodules is an
    // attractive alternative for multi-repo development:
    //      - transparent for yam: the repositories are sub-directories 
    //        of the home repo.
    //      - solves the problem of unique repository names
    //      - solves the problem of identifying repository versions
    //      - solves the problem of cloning/checking-out repository version
    //
    class __declspec(dllexport) FileRepositoryNode : public Node
    {
    public:
        FileRepositoryNode(); // needed for deserialization
        FileRepositoryNode(
            ExecutionContext* context,
            std::string const& repoName,
            std::filesystem::path const& directory,
            bool tracked);

        virtual ~FileRepositoryNode();

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

        std::string const& repoName() const;

        void directory(std::filesystem::path const& dir);
        std::filesystem::path const& directory() const;
        std::shared_ptr<DirectoryNode> directoryNode() const;

        void inputRepoNames(std::vector<std::string> const& names);
        std::vector<std::string> const& inputRepoNames() const;

        std::shared_ptr<FileExecSpecsNode> fileExecSpecsNode() const;


        std::filesystem::path symbolicDirectory() const {
            return repoNameToSymbolicPath(repoName());
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

        // Inherited from Node
        void start() override;
        std::string className() const override { return "FileRepositoryNode"; }

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamer (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private:
        std::string _repoName;
        std::filesystem::path _directory;
        bool _tracked;
        std::shared_ptr<DirectoryNode> _directoryNode;
        std::shared_ptr<FileExecSpecsNode> _fileExecSpecsNode;
        std::shared_ptr<FileRepositoryWatcher> _watcher;
        std::vector<std::string> _inputRepoNames;
    };
}
