#include "ExecutionStatistics.h"

namespace YAM
{
    ExecutionStatistics::ExecutionStatistics()
        : nStarted(0)
        , nSelfExecuted(0)
        , registerNodes(false)
        , nFileUpdates(0)
        , nDirectoryUpdates(0)
    {
    }

    void ExecutionStatistics::reset() {
        nStarted = 0;
        nSelfExecuted = 0;
        nFileUpdates = 0;
        nDirectoryUpdates = 0;
        started.clear();
        selfExecuted.clear();
        rehashedFiles.clear();
        updatedDirectories.clear();
    }

    void ExecutionStatistics::registerStarted(Node* node) {
        nStarted++;
        if (registerNodes) {
            started.insert(node);
        }
    }


    void ExecutionStatistics::registerSelfExecuted(Node* node) {
        nSelfExecuted++;
        if (registerNodes) {
            selfExecuted.insert(node);
        }
    }


    void ExecutionStatistics::registerRehashedFile(FileNode* node) {
        nFileUpdates++;
        if (registerNodes) {
            std::lock_guard<std::mutex> lock(mutex);
            rehashedFiles.insert(node);
        }
    }

    void ExecutionStatistics::registerUpdatedDirectory(DirectoryNode* node) {
        nDirectoryUpdates++;
        if (registerNodes) {
            std::lock_guard<std::mutex> lock(mutex);
            updatedDirectories.insert(node);
        }
    }
}