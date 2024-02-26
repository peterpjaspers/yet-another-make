#pragma once
#include "Node.h"

#include <filesystem>
#include <set>
#include <memory>
#include <vector>

namespace YAM
{
    class ExecutionContext;
    class DirectoryNode;

    class __declspec(dllexport) Globber
    {
    public:
        // Find nodes FileNodes and DirectoryNodes that match 'pattern'.
        // The pattern path is either a glob, e.g. src\*.cpp, or a non-glob,
        // e.g. src\main.cpp. See class Glob for supported patterns.
        // 
        // Pattern can be relative to baseDir or can be a symbolic or 
        // absolute path. An exception is thrown when there is no known
        // FileRepositoryNode that lexically contains the symbolic or absolute
        // path.
        // 
        // baseDir and pattern are optimized, see Globber::optimize. 
        // The optimized relative path pattern is matched relative to the 
        // optimized baseDir.
        //  
        // Examples:
        //    - src\*.cpp matches all cpp file nodes in baseDir\src. 
        //    - src\main.cpp matches baseDir\src\main.cpp.
        //    - ..\src\*.cpp matches all cpp file nodes in baseDir\..\src. 
        //    - given a repository with name repo and root dir C:\repoRoot
        //      then the pattern <repo>\src\*.cpp matches all cpp files in 
        //      C:\repoRoot\src
        // 
        // If 'dirsOnly' then only match directory nodes.
        // Add visited directory nodes to inputDirs. Changes in these 
        // directories will invalidate the glob result. Handling such changes
        // is out-of-scope of this class. 
        //
        // Note: Globber will match file/dir nodes in the mirrored repository
        // directory trees. This implies that Globber cannot reliably match 
        // generated file nodes because these nodes will not appear in the
        // mirrored trees until their associated files have been generated. 
        // E.g. during the first build the mirrored trees contain zero generated
        // file nodes.
        //
        Globber(
            std::shared_ptr<DirectoryNode> const& baseDir,
            std::filesystem::path const& pattern,
            bool dirsOnly
        );

        // Pattern is of form NP/GP where NP is a path without glob characters
        // and GP is a path with glob characters.
        // Post: baseDir = baseDir/NP, pattern = GP
        // baseDir/NP is canonical, i.e. does not contain .. and . components.
        static void optimize(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode>& baseDir,
            std::filesystem::path& pattern);

        // If not yet executed then execute() the glob.
        // Return the matching nodes.
        std::vector<std::shared_ptr<Node>>const& matches();

        // Execute the glob. 
        // Note: execution will occur at first call only. Result is cached
        // and can be obtained by calling matches().
        void execute();

        // Return the directory nodes visited during glob execution.
        std::set<std::shared_ptr<DirectoryNode>, Node::CompareName> const& inputDirs() const {
            return _inputDirs;
        }

        // Return the optimized baseDir and pattern.
        std::shared_ptr<DirectoryNode> const& baseDir() { return _baseDir; }
        std::filesystem::path const& pattern() { return _pattern; }

    private:

        bool isHidden(std::filesystem::path const& path);
        bool isRecursive(std::filesystem::path const& pattern);
        void walk(std::shared_ptr<DirectoryNode> const& dir);
        void match(std::filesystem::path const& pattern);
        void exists(std::filesystem::path const& file);
        std::shared_ptr<DirectoryNode> findDirectory(std::filesystem::path const& relativeToBase);

        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _pattern;
        bool _dirsOnly;
        std::set<std::shared_ptr<DirectoryNode>, Node::CompareName> _inputDirs;
        std::vector<std::shared_ptr<Node>> _matches;
        bool _executed;
    };
}

