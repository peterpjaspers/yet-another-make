#include "CollapsedFileChanges.h"


namespace
{
    using namespace YAM;

    const std::filesystem::path overflowPath("overflow");

    // previous->second.action == FileChange::Action::Added
    void collapseAdded(
        std::map<std::filesystem::path, FileChange>& changes,
        std::map<std::filesystem::path, FileChange>::iterator previous,
        FileChange const& change
    ) {
        FileChange::Action action = change.action;
        if (action == FileChange::Action::Added) {
        } else if (action == FileChange::Action::Removed) {
            previous->second.action = FileChange::Action::Removed;
        } else if (action == FileChange::Action::Modified) {
        } else if (action == FileChange::Action::Renamed) {
            // cannot happen because renames are translated to removed and added
            throw std::exception("illegal collapse change");
        }
        previous->second.lastWriteTime = change.lastWriteTime;
    }

    // previous->second.action == FileChange::Action::Removed
    void collapseRemoved(
        std::map<std::filesystem::path, FileChange>& changes,
        std::map<std::filesystem::path, FileChange>::iterator previous,
        FileChange const& change
    ) {
        FileChange::Action action = change.action;
        if (action == FileChange::Action::Added) {
            previous->second.action = FileChange::Action::Added;
        } else if (action == FileChange::Action::Removed) {
        } else if (action == FileChange::Action::Modified) {
            // should not happen, but sometimes it does
        } else if (action == FileChange::Action::Renamed) {
            // cannot happen because renames are translated to removed and added
            throw std::exception("illegal collapse change");
        }
        previous->second.lastWriteTime = change.lastWriteTime;
    }

    // previous->second.action == FileChange::Action::Modified
    void collapseModified(
        std::map<std::filesystem::path, FileChange>& changes,
        std::map<std::filesystem::path, FileChange>::iterator previous,
        FileChange const& change
    ) {
        FileChange::Action action = change.action;
        if (action == FileChange::Action::Added) {
            // Should not happen
            previous->second.action = FileChange::Action::Added;
        } else if (action == FileChange::Action::Removed) {
            previous->second.action = FileChange::Action::Removed;
        } else if (action == FileChange::Action::Modified) {
        } else if (action == FileChange::Action::Renamed) {
            // cannot happen because renames are translated to removed and added
            throw std::exception("illegal collapse change");
        }
        previous->second.lastWriteTime = change.lastWriteTime;
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
            throw std::exception("illegal collapse change");
        }
    }

    // Forward declare needed for recursive call to addChange.
    void addChange(
        std::map<std::filesystem::path, FileChange>& changes,
        FileChange const& change);

    void addRenamed(std::map<std::filesystem::path, FileChange>& changes, FileChange const& rename) {
        FileChange remove;
        remove.action = FileChange::Action::Removed;
        remove.fileName = rename.oldFileName;
        remove.lastWriteTime = rename.lastWriteTime;
        FileChange add;
        add.action = FileChange::Action::Added;
        add.fileName = rename.fileName;
        add.lastWriteTime = rename.lastWriteTime;
        addChange(changes, remove);
        addChange(changes, add);
    }

    void addChange(
        std::map<std::filesystem::path, FileChange>& changes,
        FileChange const& change
    ) {
        if (change.action == FileChange::Action::Renamed) {
            addRenamed(changes, change);
        } else {
            auto it = changes.find(change.fileName);
            if (it == changes.end()) {
                changes.insert({ change.fileName, change });
            } else {
                collapseChange(changes, it, change);
            }
        }
    }
}

namespace YAM
{
    CollapsedFileChanges::CollapsedFileChanges(std::filesystem::path const& directory)
        : _directory(directory)
    {}

    std::filesystem::path const& CollapsedFileChanges::directory() const {
        return _directory;
    }

    void CollapsedFileChanges::add(FileChange const& change) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (change.action == FileChange::Action::Overflow) {
            _changes.clear();
            _changes.insert({ overflowPath, change });
        } else if (!_changes.contains(overflowPath)) {
            FileChange absChange = change;
            absChange.fileName = _directory / change.fileName;
            if (change.action == FileChange::Action::Renamed) {
                absChange.oldFileName = _directory / absChange.oldFileName;
            }
            addChange(_changes, absChange);
        }
    }

    bool CollapsedFileChanges::hasChanged(std::filesystem::path const& path) {
        std::lock_guard<std::mutex> lock(_mutex);
        bool contains = _changes.contains(path) || _changes.contains(overflowPath);
        return contains;
    }

    void CollapsedFileChanges::consume(Delegate<void, FileChange const&> const& consumeAction) {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto const& it : _changes) {
            consumeAction.Execute(it.second);
        }
        _changes.clear();
    }

    std::map<std::filesystem::path, FileChange> const& CollapsedFileChanges::changes() {
        return _changes;
    }
}