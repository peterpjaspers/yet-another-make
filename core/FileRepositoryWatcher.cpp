#include "FileRepositoryWatcher.h"
#include "SourceDirectoryNode.h"
#include "FileNode.h"
#include "DirectoryWatcher.h"
#include "SourceFileNode.h"
#include "ExecutionContext.h"

namespace
{
    using namespace YAM;

    const std::filesystem::path overflowPath("overflow");

    bool isDirNode(Node* node) {
        return dynamic_cast<SourceDirectoryNode*>(node) != nullptr;
    }

    bool isFileNode(Node* node) {
        return dynamic_cast<FileNode*>(node) != nullptr;
    }

    bool isSubpath(std::filesystem::path const& path, std::filesystem::path const& base) {
        const auto pair = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
        return pair.second == base.end();
    }

    bool isNodeInRepo(Node* node, std::filesystem::path const& repoDir) {
        return
            (isFileNode(node) || isDirNode(node))
            && isSubpath(node->name(), repoDir);
    }
}

namespace YAM
{
    FileRepositoryWatcher::FileRepositoryWatcher(
        std::filesystem::path const& directory,
        ExecutionContext* context)
        : _context(context)
        , _changes(directory)
        , _watcher(std::make_shared<DirectoryWatcher>(
            directory,
            true,
            Delegate<void, FileChange const&>::CreateRaw(this, &FileRepositoryWatcher::_addChange))
        ) {}

    void FileRepositoryWatcher::_addChange(FileChange const& change) {
        _changes.add(change);
    }

    void FileRepositoryWatcher::consumeChanges() {
        _changes.consume(Delegate<void, FileChange const&>::CreateRaw(this, &FileRepositoryWatcher::_handleChange));
    }

    bool FileRepositoryWatcher::hasChanged(std::filesystem::path const& path) {
        return _changes.hasChanged(path);
    }

    void FileRepositoryWatcher::_handleChange(FileChange const& change) {
        if (change.fileName.filename() == "junk.txt") {
            bool stop = true;
        }
        if (change.action == FileChange::Action::Added) {
            _handleAdd(change);
        } else if (change.action == FileChange::Action::Removed) {
            _handleRemove(change);
        } else if (change.action == FileChange::Action::Modified) {
            _handleModification(change);
        } else if (change.action == FileChange::Action::Renamed) {
            // CollapsedFileChanges replaces Renameed by Removee and Added
            throw std::exception("illegal change");
        } else if (change.action == FileChange::Action::Overflow) {
            _handleOverflow();
        } else {
            throw std::exception("bad action");
        }
    }

    void FileRepositoryWatcher::_handleAdd(FileChange const& change) {
        std::filesystem::path parentDir = change.fileName.parent_path();
        // take care: cannot use change.lastWriteTime because it applies to
        // change.fileName, not to parentDir.
        std::shared_ptr<Node> node = _invalidateNode(parentDir, std::chrono::utc_clock::now());
        if (node != nullptr && dynamic_cast<SourceDirectoryNode*>(node.get()) == nullptr) {
            throw std::exception("unexpected node type");
        }
        // File nodes may exist that are associated with files that may not yet
        // exist, e.g. the file nodes for the .gitignore and .yamignore files.
        // Such nodes must be invalidated, hence call _invalidateNode.
        _invalidateNode(change.fileName, change.lastWriteTime);
    }

    void FileRepositoryWatcher::_handleRemove(FileChange const& change) {
        std::filesystem::path parentDir = change.fileName.parent_path();
        // take care: cannot use change.lastWriteTime because it applies to
        // change.fileName, not to parentDir.
        std::shared_ptr<Node> node = _invalidateNode(parentDir, std::chrono::utc_clock::now());
        if (node != nullptr && dynamic_cast<SourceDirectoryNode*>(node.get()) == nullptr) {
            throw std::exception("unexpected node type");
        }
        _invalidateNodeRecursively(change.fileName);
    }

    void FileRepositoryWatcher::_handleModification(FileChange const& change) {
        _invalidateNode(change.fileName, change.lastWriteTime);
    }

    void FileRepositoryWatcher::_handleOverflow() {
        std::vector<std::shared_ptr<Node>> nodesInRepo;
        std::filesystem::path const& repoDir = _changes.directory();
        auto includeNode = Delegate<bool, std::shared_ptr<Node> const&>::CreateLambda(
            [&repoDir](std::shared_ptr<Node> const& node) {
                return isNodeInRepo(node.get(), repoDir);
            });
        _context->nodes().find(includeNode, nodesInRepo);
        for (auto node : nodesInRepo) {
            node->setState(Node::State::Dirty);
        }
    }

    std::shared_ptr<Node> FileRepositoryWatcher::_invalidateNode(
        std::filesystem::path const& path,
        std::chrono::time_point<std::chrono::utc_clock> const& lastWriteTime
    ) {
        std::shared_ptr<Node> node = _context->nodes().find(path);
        if (node != nullptr) {
            bool dirty = true;
            auto fileNode = dynamic_pointer_cast<FileNode>(node);
            if (fileNode != nullptr) {
                dirty = fileNode->lastWriteTime() != lastWriteTime;
            } else {
                auto dirNode = dynamic_pointer_cast<SourceDirectoryNode>(node);
                if (dirNode != nullptr) {
                    dirty = dirNode->lastWriteTime() != lastWriteTime;
                }
            }
            if (dirty) {
                node->setState(Node::State::Dirty);
            }
        }
        return node;
    }

    void FileRepositoryWatcher::_invalidateNodeRecursively(std::filesystem::path const& path) {
        std::shared_ptr<Node> node = _context->nodes().find(path);
        if (node != nullptr) _invalidateNodeRecursively(node);
    }

    void FileRepositoryWatcher::_invalidateNodeRecursively(std::shared_ptr<Node> const& node) {
        node->setState(Node::State::Dirty);
        auto dirNode = dynamic_pointer_cast<SourceDirectoryNode>(node);
        if (dirNode != nullptr) {
            for (auto const& pair : dirNode->getContent()) {
                _invalidateNodeRecursively(pair.second);
            }
        }
    }
}