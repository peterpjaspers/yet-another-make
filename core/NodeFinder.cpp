#include "NodeFinder.h"
#include "Node.h"
#include "FileNode.h"
#include "DirectoryNode.h"
#include "ExecutionContext.h"
#include "FileRepository.h"
#include "Glob.h"

namespace
{
    using namespace YAM;
    namespace fs = std::filesystem;
}

namespace YAM
{
    NodeFinder::NodeFinder(
        ExecutionContext* context,
        std::shared_ptr<DirectoryNode>& baseDir,
        std::filesystem::path const& pattern,
        bool dirsOnly,
        std::set<std::shared_ptr<DirectoryNode>>& inputDirs
    )
        : _context(_context)
        , _baseDir(baseDir)
        , _pattern(pattern)
        , _dirsOnly(dirsOnly)
        , _inputDirs(inputDirs)
    {
        _inputDirs.insert(_baseDir);

        std::filesystem::path dirPattern = pattern.parent_path();
        std::filesystem::path filePattern = pattern.filename();        
        if (dirPattern.empty()) {
            if (isRecursive(filePattern)) {
                walk(_baseDir.get());
            } else if (Glob::isGlob(filePattern.string())) {
                match(filePattern);
            } else {
                exists(filePattern);
            }
        } else if (Glob::isGlob(dirPattern.string())) {
            NodeFinder finder(_context, _baseDir, dirPattern, true, _inputDirs);
            for (auto const& m : finder._matches) {
                auto dirNode = dynamic_pointer_cast<DirectoryNode>(m);
                NodeFinder mfinder(_context, dirNode, filePattern, _dirsOnly, _inputDirs);
                _matches = mfinder._matches;
            }
        } else {
            auto dirNode = findDirectory(dirPattern);
            if (dirNode != nullptr) {
                NodeFinder finder(_context, dirNode, filePattern, _dirsOnly, _inputDirs);
                _matches = finder._matches;
            }
        }
    }

    bool NodeFinder::isHidden(std::filesystem::path const& path) { return path.string()[0] == '.'; }
    bool NodeFinder::isRecursive(std::filesystem::path const& pattern) { return pattern == "**"; }

    void NodeFinder::walk(DirectoryNode* dir) {
        for (auto const& pair : dir->getContent()) {
            auto const& child = pair.second;
            auto subDir = dynamic_pointer_cast<DirectoryNode>(child);
            if (_dirsOnly) {
                if (subDir != nullptr) {
                    _matches.push_back(child);
                }
            } else {
                _matches.push_back(child);
            }
            if (subDir != nullptr) {
                walk(subDir.get());
            }
        }
    }

    void NodeFinder::match(std::filesystem::path const& pattern) {
        Glob glob(pattern);
        for (auto const& pair : _baseDir->getContent()) {
            auto const& child = pair.second;
            if (glob.matches(pair.first.filename())) {
                if (_dirsOnly) {
                    if (dynamic_pointer_cast<DirectoryNode>(child) != nullptr) {
                        _matches.push_back(child);
                    }
                } else {
                    _matches.push_back(child);
                }
            }
        }
    }

    void NodeFinder::exists(std::filesystem::path const& file) {
        std::shared_ptr<Node> node = _baseDir->findChild(file);
        if (node != nullptr) _matches.push_back(node);
    }

    // path is a symbolic path or a path relative to _baseDir
    std::shared_ptr<DirectoryNode> NodeFinder::findDirectory(std::filesystem::path const& path) {
        std::shared_ptr<Node> node = _baseDir->findChild(path);
        return dynamic_pointer_cast<DirectoryNode>(node);
    }


}