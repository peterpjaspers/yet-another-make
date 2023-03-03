#include "ExecutionContext.h"
#include "Node.h"
#include "SourceFileRepository.h"
#include "BuildRequest.h"
#include "ConsoleLogBook.h"

namespace
{
    using namespace YAM;
    // TODO: retrieve number of cores from operating system
    std::size_t getDefaultPoolSize() {
        return 4;
    }

    Delegate<bool, std::shared_ptr<Node> const&> includeIfDirty = 
        Delegate<bool, std::shared_ptr<Node> const&>::CreateLambda(
            [](std::shared_ptr<Node> const& node) 
            { 
                return node->state() == Node::State::Dirty; 
            });
}

namespace YAM
{

    ExecutionContext::ExecutionContext()
        : _mainThread(&_mainThreadQueue, "YAM_main")
        , _threadPool(&_threadPoolQueue, "YAM_threadpool", getDefaultPoolSize()) 
        , _logBook(std::make_shared<ConsoleLogBook>())
    {
        auto const& entireFileSet = FileAspectSet::entireFileSet();
        _fileAspectSets.insert({ entireFileSet.name(), entireFileSet});
    }

    ExecutionContext::~ExecutionContext() {
        // destroy repositories to stop directory watching before stopping 
        // main thread
        _repositories.clear();
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

    bool ExecutionContext::addRepository(std::shared_ptr<FileRepository> repo)
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

    std::shared_ptr<FileRepository> ExecutionContext::findRepository(std::string const& repoName) const {
        auto it = _repositories.find(repoName);
        bool found = it != _repositories.end();
        if (found) return it->second;
        return nullptr;
    }

    std::shared_ptr<FileRepository> ExecutionContext::findRepositoryContaining(std::filesystem::path const& path) const {
        for (auto pair : _repositories) {
            auto repo = pair.second;
            if (repo->lexicallyContains(path)) return repo;
        }
        return nullptr;
    }

    std::map<std::string, std::shared_ptr<FileRepository>> const& ExecutionContext::repositories() const {
        return _repositories;
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

    void ExecutionContext::getDirtyNodes(std::vector<std::shared_ptr<Node>>& dirtyNodes) {
        _nodes.find(includeIfDirty, dirtyNodes);
    }

    void ExecutionContext::buildRequest(std::shared_ptr<BuildRequest> request) {
        _request = request;
    }

    void ExecutionContext::logBook(std::shared_ptr<ILogBook> newBook) {
        _logBook = newBook;
    }

    std::shared_ptr<ILogBook> ExecutionContext::logBook() const {
        return _logBook;
    }
    void ExecutionContext::addToLogBook(LogRecord const& record) {
        _logBook->add(record);
    }

    std::shared_ptr<BuildRequest> ExecutionContext::buildRequest() const {
        return _request;
    }
}