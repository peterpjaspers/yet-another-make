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

    void canonicalize(std::shared_ptr<DirectoryNode>& baseDir, std::filesystem::path& pattern) {
        std::filesystem::path patternDirPath;
        auto it = pattern.begin();
        for (; it != pattern.end() && !Glob::isGlob(it->string()); ++it) {
            auto tmp = dynamic_pointer_cast<DirectoryNode>(baseDir->findChild(*it));
            if (tmp == nullptr) break;
            baseDir = tmp;
        };
        std::filesystem::path newPattern;
        for (; it != pattern.end(); ++it) {
            newPattern /= *it;
        }
        pattern = newPattern;
    }
}

namespace YAM
{
    Globber::Globber(
        ExecutionContext* context,
        std::shared_ptr<DirectoryNode> const& baseDir,
        std::filesystem::path const& pattern,
        bool dirsOnly,
        std::set<std::shared_ptr<DirectoryNode>, Node::CompareName>& inputDirs
    )
        : _baseDir(baseDir)
        , _pattern(pattern)
        , _dirsOnly(dirsOnly)
        , _inputDirs(inputDirs)
    {
        std::string repoName = FileRepository::repoNameFromPath(_pattern);
        if (!repoName.empty()) {
            auto repo = context->findRepository(repoName);
            if (repo != nullptr) {
                _baseDir = repo->directoryNode();
                _pattern = repo->relativePathOf(repo->absolutePathOf(_pattern));
            }
        }
        canonicalize(_baseDir, _pattern);
        _inputDirs.insert(_baseDir);

        std::filesystem::path dirPattern = _pattern.parent_path();
        std::filesystem::path filePattern = _pattern.filename();        
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
        _inputDirs.insert(dir);
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

    // path is a symbolic path or a path relative to _baseDir
    std::shared_ptr<Node> Globber::findNode(std::filesystem::path const& path) {
        std::shared_ptr<Node> node;
        if (FileRepository::isSymbolicPath(path)) {
            node = _baseDir->context()->nodes().find(path);
        } else if (path.is_absolute()) {
            std::shared_ptr<FileRepository> repo = _baseDir->context()->findRepositoryContaining(path);
            if (repo != nullptr) {
                std::filesystem::path abcCanonicalPath = std::filesystem::canonical(path);
                std::filesystem::path symPath = _baseDir->repository()->symbolicPathOf(abcCanonicalPath);
                node = _baseDir->context()->nodes().find(symPath);    
            }
        } else {
            std::filesystem::path absPath(_baseDir->absolutePath() / path);
            std::filesystem::path abcCanonicalPath = std::filesystem::canonical(absPath);
            std::filesystem::path symPath = _baseDir->repository()->symbolicPathOf(abcCanonicalPath);
            node = _baseDir->context()->nodes().find(symPath);
        }
        return node;
    }

    // path is a symbolic path or a path relative to _baseDir
    void Globber::exists(std::filesystem::path const& file) {
        if (file.empty()) {
            _matches.push_back(_baseDir);
        } else {
            //std::shared_ptr<Node> node = _baseDir->findChild(file);
            std::shared_ptr<Node> node = findNode(file);
            if (node != nullptr) {
                _matches.push_back(node);
            }
        } 
    }

    // path is a symbolic path or a path relative to _baseDir
    std::shared_ptr<DirectoryNode> Globber::findDirectory(std::filesystem::path const& path) {
        if (path == _baseDir->name()) return _baseDir;
        //std::shared_ptr<Node> node = _baseDir->findChild(path);
        std::shared_ptr<Node> node = findNode(path);
        return dynamic_pointer_cast<DirectoryNode>(node);
    }
}