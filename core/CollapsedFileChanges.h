#pragma once

#include "IDirectoryWatcher.h"

#include <filesystem>
#include <map>
#include <mutex>

namespace YAM
{
    class __declspec(dllexport) CollapsedFileChanges
    {
    public:
        // Construct a set of file changes.
        CollapsedFileChanges();

        // Add a change. Thread-safe.
        void add(FileChange const& change);

        // Return whether a change is contained for path.
        // Thread-safe
        bool hasChanged(std::filesystem::path const& path);

        // Perform for each change the given consumeAction.
        // Clear changes after all actions have been performed.
        // Thread-safe
        void consume(Delegate<void, FileChange const&> const& consumeAction);

        // Return the map containing the collapsed file changes.
        // fileName/oldFileName in this map are absolute path names.
        // Not thread-safe, intended for testing purposes.
        std::map<std::filesystem::path, FileChange> const& changes();

    public:
        std::mutex _mutex;
        std::map<std::filesystem::path, FileChange> _changes;
    };
}