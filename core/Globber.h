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
        // The pattern is either a glob, e.g. src\*.cpp, or a non-glob, 
        // e.g. src\main.cpp. See class Glob for supported patterns.
        // The pattern can be a path relative to 'baseDir' or a symbolic
        // path. In the latter case baseDir is ignored.
        // If the pattern is a relative path then it is matched relative 
        // to 'baseDir'. 
        // If the pattern is a symbolic path then baseDir is ignored and the 
        // relative part of pattern is matched against the repo root directory.
        // Examples:
        //    - src\*.cpp matches all cpp file nodes in baseDir\src. 
        //    - src\main.cpp matches baseDir\src\main.cpp.
        //    - ..\src\*.cpp matches all cpp file nodes in baseDir\..\src. 
        //    - given a repository with name repo and root dir C:\repoRoot
        //      then the pattern <repo>\src\*.cpp matches all cpp files in 
        //      C:\repoRoot\src
        // If 'dirsOnly' then only match directory nodes.
        // Add visited directory nodes to inputDirs.
        Globber(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            std::filesystem::path const& pattern,
            bool dirsOnly,
            std::set<std::shared_ptr<DirectoryNode>, Node::CompareName>& inputDirs
        );
        std::vector<std::shared_ptr<Node>> const& matches() { return _matches; }

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
        std::set<std::shared_ptr<DirectoryNode>, Node::CompareName>& _inputDirs;
        std::vector<std::shared_ptr<Node>> _matches;
    };
}

