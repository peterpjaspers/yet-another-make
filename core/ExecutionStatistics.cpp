#include "ExecutionStatistics.h"

namespace YAM
{
    ExecutionStatistics::ExecutionStatistics()
        : nStarted(0)
        , nSelfExecuted(0)
        , registerNodes(false) {
    }

    void ExecutionStatistics::reset() {
        nStarted = 0;
        nSelfExecuted = 0;
        started.clear();
        selfExecuted.clear();
    }

    void ExecutionStatistics::registerStarted(Node* node) {
        nStarted++;
        if (registerNodes) started.insert(node);
    }

    void ExecutionStatistics::registerSelfExecuted(Node* node) {
        nSelfExecuted++;
        if (registerNodes) selfExecuted.insert(node);
    }
}