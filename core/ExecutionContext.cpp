#include "ExecutionContext.h"

namespace
{
    using namespace YAM;
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
    {
        auto const& entireFileSet = FileAspectSet::entireFileSet();
        _fileAspectSets.insert({ entireFileSet.name(), entireFileSet});
    }

    ExecutionContext::~ExecutionContext() {
        _mainThreadQueue.stop(); // this will cause _mainThread to finish
    }

    ThreadPool& ExecutionContext::threadPool() {
        return _threadPool;
    }
    Thread& ExecutionContext::mainThread() {
        return _mainThread;
    }
    Dispatcher& ExecutionContext::threadPoolQueue() {
        return _threadPoolQueue;
    }
    Dispatcher& ExecutionContext::mainThreadQueue()  {
        return _mainThreadQueue;
    }

    ExecutionStatistics& ExecutionContext::statistics() {
        return _statistics;
    }

    // Return the file aspects applicable to the file with the given path name.
    // Order the aspects in the returned vector by ascending aspect name.
    std::vector<FileAspect> ExecutionContext::findFileAspects(std::filesystem::path const& path) const {
        std::vector<FileAspect> applicableAspects;
        applicableAspects.push_back(FileAspect::entireFileAspect());
        return applicableAspects;
    }

    // Return the file aspect set identified by the given name.
    // Throw exception when no such set exists.
    FileAspectSet const& ExecutionContext::findFileAspectSet(std::string const& aspectSetName) const {

        auto const& s = _fileAspectSets.find(aspectSetName);
        if (s == _fileAspectSets.cend()) throw std::runtime_error("no such FileAspectSet");
        return s->second;
    }

    NodeSet& ExecutionContext::nodes() {
        return _nodes;
    }
}