#pragma once

#include <unordered_set>
#include <mutex>
#include <atomic>

namespace YAM
{
    class Node;
    class FileNode;
    class DirectoryNode;

    class __declspec(dllexport) ExecutionStatistics
    {
    public:
        ExecutionStatistics();

        void reset();

        void registerStarted(Node const* node);
        void registerSelfExecuted(Node const* node);

        void registerRehashedFile(FileNode const* node);
        void registerUpdatedDirectory(DirectoryNode const* node);

        // number of nodes started
        unsigned int nStarted;
        // number of nodes self-executed
        unsigned int nSelfExecuted;

        // true => fill sets, else only increment counters
        std::atomic<bool> registerNodes;

        std::unordered_set<Node const*> started;
        std::unordered_set<Node const*> selfExecuted;

        // These two fields are incremented from threadpool,
        // hence use atomics.
        std::atomic<unsigned int> nRehashedFiles;
        std::atomic<unsigned int> nDirectoryUpdates;

        std::mutex mutex;
        // These 2 fields are updated from threadpool.
        // Use mutex to do MT-safe update.
        std::unordered_set<FileNode const*> rehashedFiles;
        std::unordered_set<DirectoryNode const*> updatedDirectories;
    };
}
