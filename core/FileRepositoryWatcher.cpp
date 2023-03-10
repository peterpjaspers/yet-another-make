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

    void insertRenamed(std::map<std::filesystem::path, FileChange>& changes, FileChange const& rename) {
        FileChange remove;
        remove.action = FileChange::Action::Removed;
        remove.fileName = rename.oldFileName;
        remove.lastWriteTime = rename.lastWriteTime;
        FileChange add;
        add.action = FileChange::Action::Added;
        add.fileName = rename.fileName;
        add.lastWriteTime = rename.lastWriteTime;
        changes.insert({ remove.fileName, remove });
        changes.insert({ add.fileName, add });
    }

    void insertChange(std::map<std::filesystem::path, FileChange>& changes, FileChange const& change) {
        if (change.action == FileChange::Action::Renamed) {
            insertRenamed(changes, change);
        } else {
            changes.insert({ change.fileName, change });
        }
    }

    // previous->second.action == FileChange::Action::Added
    void collapseAdded(
        std::map<std::filesystem::path, FileChange>& changes,
        std::map<std::filesystem::path, FileChange>::iterator previous,
        FileChange const& change
    ) {
        FileChange::Action action = change.action;
        if (action == FileChange::Action::Added) {
            previous->second.lastWriteTime = change.lastWriteTime;
        } else if (action == FileChange::Action::Removed) {
            previous->second.action = FileChange::Action::Removed;
            previous->second.lastWriteTime = change.lastWriteTime;
        } else if (action == FileChange::Action::Modified) {
            previous->second.lastWriteTime = change.lastWriteTime;
        } else if (action == FileChange::Action::Renamed) {
            changes.erase(previous);
            insertRenamed(changes, change);
        }
    }

    // previous->second.action == FileChange::Action::Removed
    void collapseRemoved(
        std::map<std::filesystem::path, FileChange>& changes,
        std::map<std::filesystem::path, FileChange>::iterator previous,
        FileChange const& change
    ) {
        FileChange::Action action = change.action;
        if (action == FileChange::Action::Added) {
            previous->second.action = FileChange::Action::Modified;
            previous->second.lastWriteTime = change.lastWriteTime;
        } else if (action == FileChange::Action::Removed) {
            previous->second.lastWriteTime = change.lastWriteTime;
        } else if (action == FileChange::Action::Modified) {
        } else if (action == FileChange::Action::Renamed) {
            previous->second.lastWriteTime = change.lastWriteTime;
        }
    }

    // previous->second.action == FileChange::Action::Modified
    void collapseModified(
        std::map<std::filesystem::path, FileChange>& changes,
        std::map<std::filesystem::path, FileChange>::iterator previous,
        FileChange const& change
    ) {
        FileChange::Action action = change.action;
        if (action == FileChange::Action::Added) {
        } else if (action == FileChange::Action::Removed) {
            previous->second.action = FileChange::Action::Removed;
            previous->second.lastWriteTime = change.lastWriteTime;
        } else if (action == FileChange::Action::Modified) {
            previous->second.lastWriteTime = change.lastWriteTime;
        } else if (action == FileChange::Action::Renamed) {
            changes.erase(previous);
            insertRenamed(changes, change);
        }
    }

    void collapseChange(
        std::map<std::filesystem::path, FileChange>& changes,
        std::map<std::filesystem::path, FileChange>::iterator previous,
        FileChange const& change
    ) {
        FileChange::Action pa = previous->second.action;
        if (pa == FileChange::Action::Added) {
            collapseAdded(changes, previous, change);
        } else if (pa == FileChange::Action::Removed) {
            collapseRemoved(changes, previous, change);
        } else if (pa == FileChange::Action::Modified) {
            collapseModified(changes, previous, change);
        } else if (pa == FileChange::Action::Renamed) {
            // cannot happen because renames are translated to removed and added
            throw std::exception("illegal change transition");
        }
    }
}

namespace YAM
{
    FileRepositoryWatcher::FileRepositoryWatcher(
        std::filesystem::path const& directory,
        ExecutionContext* context)
        : _directory(directory)
        , _context(context)
        , _watcher(std::make_shared<DirectoryWatcher>(
            directory,
            true,
            Delegate<void, FileChange const&>::CreateRaw(this, &FileRepositoryWatcher::_addChange))
        ) {}

    void FileRepositoryWatcher::_addChange(FileChange const& change) {
        if (change.fileName.filename() == "junk.txt") {
            bool stop = true;
        }
        std::lock_guard<std::mutex> lock(_mutex);
        if (change.action == FileChange::Action::Overflow) {
            _changes.clear();
            _changes.insert({ overflowPath, change });
        } else if (!_changes.contains(overflowPath)) {
            FileChange absChange = change;
            absChange.fileName = _directory / change.fileName;
            if (!absChange.oldFileName.empty()) absChange.oldFileName = _directory / absChange.oldFileName;
            auto it = _changes.find(absChange.fileName);
            if (it == _changes.end()) {
                insertChange(_changes, absChange);
            } else {
                collapseChange(_changes, it, absChange);
            }
        }
    }

    void FileRepositoryWatcher::consumeChanges() {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto const& pair : _changes) {
            _handleChange(pair.second);
        }
        _changes.clear();
    }

    bool FileRepositoryWatcher::hasChanged(std::filesystem::path const& path) {
        std::lock_guard<std::mutex> lock(_mutex);
        bool contains = _changes.contains(path) || _changes.contains(overflowPath);
        return contains;
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
        std::filesystem::path const& repoDir = _directory;
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