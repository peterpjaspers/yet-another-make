#include "Globber.h"
#include "Node.h"
#include "FileNode.h"
#include "DirectoryNode.h"
#include "ExecutionContext.h"
#include "FileRepositoryNode.h"
#include "Glob.h"

namespace
{
    using namespace YAM;
    namespace fs = std::filesystem;

    void resolveSymbolicOrAbsolutePath(
        ExecutionContext* context,
        std::shared_ptr<DirectoryNode>& baseDir,
        std::filesystem::path& pattern)
    {
        if (pattern.is_absolute()) {
            auto repo = baseDir->context()->findRepositoryContaining(pattern);
            if (repo != nullptr) {
                baseDir = repo->directoryNode();
                pattern = repo->relativePathOf(pattern);
            } else {
                throw std::runtime_error(pattern.string() + " is not a path in a known repository");
            }
        } else {
            std::string repoName = FileRepositoryNode::repoNameFromPath(pattern);
            if (!repoName.empty()) {
                auto repo = context->findRepository(repoName);
                if (repo == nullptr) {
                    throw std::runtime_error(pattern.string() + " is not a path in a known repository");
                } else if (repo->repoType() != FileRepositoryNode::RepoType::Ignore) {
                    baseDir = repo->directoryNode();
                    pattern = repo->relativePathOf(repo->absolutePathOf(pattern));
                } else {
                    throw std::runtime_error(pattern.string() + " is a path in an Ignored repository");
                }
            }
        }
    }
}

namespace YAM
{
    Globber::Globber(
        std::shared_ptr<DirectoryNode> const& baseDir,
        std::filesystem::path const& pattern,
        bool dirsOnly
    )
        : _baseDir(baseDir)
        , _pattern(pattern)
        , _dirsOnly(dirsOnly)
        , _executed(false)
    {
        optimize(_baseDir->context(), _baseDir, _pattern);
    }


    std::vector<std::shared_ptr<Node>>const& Globber::matches() {
        execute();
        return _matches;
    }

    void Globber::execute() {
        if (_executed) return;
        _executed = true;

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
            Globber finder(_baseDir, dirPattern, true);
            if (filePattern.empty()) {
                _matches.insert(_matches.end(), finder.matches().begin(), finder.matches().end());
                _inputDirs.insert(finder.inputDirs().begin(), finder.inputDirs().end());
            } else {
                for (auto const& m : finder.matches()) {
                    auto dirNode = dynamic_pointer_cast<DirectoryNode>(m);
                    Globber mfinder(dirNode, filePattern, _dirsOnly);
                    _matches.insert(_matches.end(), mfinder.matches().begin(), mfinder.matches().end());
                    _inputDirs.insert(mfinder.inputDirs().begin(), mfinder.inputDirs().end());
                }
            }
        } else {
            auto dirNode = findDirectory(dirPattern);
            if (dirNode != nullptr) {
                Globber finder(dirNode, filePattern, _dirsOnly);
                _matches = finder.matches();
                _inputDirs = finder.inputDirs();
            }
        }
    }

    void Globber::optimize(
        ExecutionContext* context,
        std::shared_ptr<DirectoryNode>& baseDir, 
        std::filesystem::path& pattern
    ) {
        resolveSymbolicOrAbsolutePath(context, baseDir, pattern);

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

    std::shared_ptr<DirectoryNode> Globber::findDirectory(std::filesystem::path const& path) {
        if (path == _baseDir->name()) return _baseDir;
        std::shared_ptr<Node> node = _baseDir->findChild(path);
        return dynamic_pointer_cast<DirectoryNode>(node);
    }
}