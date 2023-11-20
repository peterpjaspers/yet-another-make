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

    class __declspec(dllexport) NodeFinder
    {
    public:
        // Find nodes File and Directory nodes that match 'pattern'.
        // The pattern is relative to 'baseDir'.
        // If 'dirsOnly' then only match directory nodes.
        // Add visited directory nodes to inputDirs.
        NodeFinder(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode>& baseDir,
            std::filesystem::path const& pattern,
            bool dirsOnly,
            std::set<std::shared_ptr<DirectoryNode>>& inputDirs
        );
        std::vector<std::shared_ptr<Node>> const& matches() { return _matches; }

    private:
        bool isHidden(std::filesystem::path const& path);
        bool isRecursive(std::filesystem::path const& pattern);
        void walk(DirectoryNode* dir);
        void match(std::filesystem::path const& pattern);
        void exists(std::filesystem::path const& file);
        std::shared_ptr<DirectoryNode> findDirectory(std::filesystem::path const& relativeToBase);

        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _pattern;
        bool _dirsOnly;
        std::set<std::shared_ptr<DirectoryNode>>& _inputDirs;
        std::vector<std::shared_ptr<Node>> _matches;
    };
}

