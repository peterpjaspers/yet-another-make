#include "Globber.h"
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
    // TODO: handle symbolic path patterns
    //
    Globber::Globber(
        ExecutionContext* context,
        std::shared_ptr<DirectoryNode> const& baseDir,
        std::filesystem::path const& pattern,
        bool dirsOnly,
        std::set<std::shared_ptr<DirectoryNode>>& inputDirs
    )
        : _baseDir(baseDir)
        , _pattern(pattern)
        , _dirsOnly(dirsOnly)
        , _inputDirs(inputDirs)
    {
        auto repo = context->findRepositoryContaining(pattern);
        if (repo != nullptr) {
            _baseDir = repo->directoryNode();
            _pattern = repo->relativePathOf(repo->absolutePathOf(pattern));
        }
        _inputDirs.insert(_baseDir);

        std::filesystem::path dirPattern = pattern.parent_path();
        std::filesystem::path filePattern = pattern.filename();        
        if (dirPattern.empty()) {
            if (isRecursive(filePattern)) {
                walk(_baseDir);
            } else if (Glob::isGlob(filePattern.string())) {
                match(filePattern);
            } else {
                exists(filePattern);
            }
        } else if (Glob::isGlob(dirPattern.string())) {
            Globber finder(context, _baseDir, dirPattern, true, _inputDirs);
            if (filePattern.empty()) {
                _matches.insert(_matches.end(), finder._matches.begin(), finder._matches.end());
            } else {
                for (auto const& m : finder._matches) {
                    auto dirNode = dynamic_pointer_cast<DirectoryNode>(m);
                    Globber mfinder(context, dirNode, filePattern, _dirsOnly, _inputDirs);
                    _matches.insert(_matches.end(), mfinder._matches.begin(), mfinder._matches.end());
                }
            }
        } else {
            auto dirNode = findDirectory(dirPattern);
            if (dirNode != nullptr) {
                Globber finder(context, dirNode, filePattern, _dirsOnly, _inputDirs);
                _matches = finder._matches;
            }
        }
    }

    bool Globber::isHidden(std::filesystem::path const& path) { return path.string()[0] == '.'; }
    bool Globber::isRecursive(std::filesystem::path const& pattern) { return pattern == "**"; }

    void Globber::walk(std::shared_ptr<DirectoryNode> const& dir) {
        _matches.push_back(dir);
        for (auto const& pair : dir->getContent()) {
            auto const& child = pair.second;
            auto subDir = dynamic_pointer_cast<DirectoryNode>(child);
            if (subDir != nullptr) {
                walk(subDir);
            } else if (!_dirsOnly) {
                _matches.push_back(child);
            }
        }
    }

    void Globber::match(std::filesystem::path const& pattern) {
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

    void Globber::exists(std::filesystem::path const& file) {
        if (file.empty()) {
            _matches.push_back(_baseDir);
        } else {
            std::shared_ptr<Node> node = _baseDir->findChild(file);
            if (node != nullptr) {
                _matches.push_back(node);
            }
        } 
    }

    // path is a symbolic path or a path relative to _baseDir
    std::shared_ptr<DirectoryNode> Globber::findDirectory(std::filesystem::path const& path) {
        std::shared_ptr<Node> node = _baseDir->findChild(path);
        return dynamic_pointer_cast<DirectoryNode>(node);
    }
}