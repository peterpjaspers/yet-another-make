#include "ExecutionContext.h"

namespace
{
    // TODO: retrieve number of cores from operating system
    std::size_t getDefaultPoolSize() {
        return 1; // 4;
    }
}

namespace YAM
{
    ExecutionContext::ExecutionContext()
        : _mainThread(&_mainThreadQueue, "YAM_main")
        , _threadPool(&_threadPoolQueue, "YAM_threadpool", getDefaultPoolSize()) 
    {}

    ExecutionContext::~ExecutionContext() {
        _mainThreadQueue.stop(); // this will cause _mainThread to finish
    }

    ThreadPool& ExecutionContext::threadPool() {
        return _threadPool;
    }
    Dispatcher& ExecutionContext::threadPoolQueue() {
        return _threadPoolQueue;
    }
    Dispatcher& ExecutionContext::mainThreadQueue() {
        return _mainThreadQueue;
    }

    ExecutionStatistics& ExecutionContext::statistics() {
        return _statistics;
    }

    NodeSet& ExecutionContext::nodes() {
        return _nodes;
    }
}