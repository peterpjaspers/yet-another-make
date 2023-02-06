#include "ExecutionContext.h"
#include "Node.h"
#include "SourceFileRepository.h"

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

    bool ExecutionContext::addRepository(std::shared_ptr<SourceFileRepository> repo)
    {
        bool duplicateName = nullptr != findRepository(repo->name());
        if (!duplicateName) {
            _repositories.insert({ repo->name(), repo });
        }
        return !duplicateName;
    }

    bool ExecutionContext::removeRepository(std::string const& repoName) {
        auto it = _repositories.find(repoName);
        bool found = it != _repositories.end();
        if (found) {
            _repositories.erase(it);
        }
        return found;
    }

    std::shared_ptr<SourceFileRepository> ExecutionContext::findRepository(std::string const& repoName) const {
        auto it = _repositories.find(repoName);
        bool found = it != _repositories.end();
        if (found) return it->second;
        return nullptr;
    }

    std::shared_ptr<SourceFileRepository> ExecutionContext::findRepository(std::filesystem::path const& path) const {
        for (auto pair : _repositories) {
            auto repo = pair.second;
            if (repo->contains(path)) return repo;
        }
        return nullptr;
    }

    std::map<std::string, std::shared_ptr<SourceFileRepository>> const& ExecutionContext::repositories() const {
        return _repositories;
    }

    // TODO: remove this function once DirectoryNode takes excludes patterns from .yamignore
    // file.
    RegexSet const& ExecutionContext::findExcludes(std::filesystem::path const& path) const {
        static RegexSet noExcludes;
        auto repo = findRepository(path);
        if (repo != nullptr) return repo->excludes();
        return noExcludes;
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

    NodeSet & ExecutionContext::nodes() {
        return _nodes;
    }

    void ExecutionContext::getDirtyNodes(std::vector<std::shared_ptr<Node>>& dirtyNodes) const {
        dirtyNodes.clear();
        for (auto const& pair : _nodes.nodes()) {
            auto node = pair.second;
            if (node->state() == Node::State::Dirty) {
                dirtyNodes.push_back(node);
            }
        }
    }

    void ExecutionContext::getDirtyNodes(std::map<std::filesystem::path, std::shared_ptr<Node>>& dirtyNodes) const {
        dirtyNodes.clear();
        for (auto const& pair : _nodes.nodes()) {
            auto node = pair.second;
            if (node->state() == Node::State::Dirty) {
                dirtyNodes.insert(pair);
            }
        }
    }
}