#include "ExecutionStatistics.h"

namespace YAM
{
    ExecutionStatistics::ExecutionStatistics()
        : nStarted(0)
        , nSelfExecuted(0)
        , registerNodes(false)
        , nRehashedFiles(0)
        , nDirectoryUpdates(0)
    {
    }

    void ExecutionStatistics::reset() {
        nStarted = 0;
        nSelfExecuted = 0;
        nRehashedFiles = 0;
        nDirectoryUpdates = 0;
        started.clear();
        selfExecuted.clear();
        rehashedFiles.clear();
        updatedDirectories.clear();
    }

    void ExecutionStatistics::registerStarted(Node const* node) {
        nStarted++;
        if (registerNodes) {
            started.insert(node);
        }
    }


    void ExecutionStatistics::registerSelfExecuted(Node const* node) {
        nSelfExecuted++;
        if (registerNodes) {
            selfExecuted.insert(node);
        }
    }


    void ExecutionStatistics::registerRehashedFile(FileNode const* node) {
        nRehashedFiles++;
        if (registerNodes) {
            std::lock_guard<std::mutex> lock(mutex);
            rehashedFiles.insert(node);
        }
    }

    void ExecutionStatistics::registerUpdatedDirectory(DirectoryNode const* node) {
        nDirectoryUpdates++;
        if (registerNodes) {
            std::lock_guard<std::mutex> lock(mutex);
            updatedDirectories.insert(node);
        }
    }
}