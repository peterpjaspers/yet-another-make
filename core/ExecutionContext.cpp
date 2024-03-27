#include "ExecutionContext.h"
#include "Node.h"
#include "RepositoriesNode.h"
#include "FileRepositoryNode.h"
#include "BuildRequest.h"
#include "ConsoleLogBook.h"

namespace
{
    using namespace YAM;

    uint32_t nPriorities() { return 32; }

    std::size_t getDefaultPoolSize() {
        static unsigned int n = std::thread::hardware_concurrency();
        return n;
    }

    Delegate<bool, std::shared_ptr<Node> const&> includeIfDirty = 
        Delegate<bool, std::shared_ptr<Node> const&>::CreateLambda(
            [](std::shared_ptr<Node> const& node) 
            { 
                return node->state() == Node::State::Dirty; 
            });


    static std::shared_ptr<FileRepositoryNode> nullRepo;
}

namespace YAM
{

    ExecutionContext::ExecutionContext()
        : _mainThreadQueue(nPriorities())
        , _threadPoolQueue(nPriorities())
        , _mainThread(&_mainThreadQueue, "YAM_main")
        , _threadPool(&_threadPoolQueue, "YAM_threadpool", getDefaultPoolSize()) 
        , _logBook(std::make_shared<ConsoleLogBook>())
    {
        auto const& entireFileSet = FileAspectSet::entireFileSet();
        _fileAspectSets.insert({ entireFileSet.name(), entireFileSet});
    }

    ExecutionContext::~ExecutionContext() {
        for (auto const& pair : repositories()) pair.second->stopWatching();
        _mainThreadQueue.stop(); // this will cause _mainThread to finish
    }

    ThreadPool& ExecutionContext::threadPool() {
        return _threadPool;
    }
    Thread& ExecutionContext::mainThread() {
        return _mainThread;
    }
    PriorityDispatcher& ExecutionContext::threadPoolQueue() {
        return _threadPoolQueue;
    }
    PriorityDispatcher& ExecutionContext::mainThreadQueue()  {
        return _mainThreadQueue;
    }

    void ExecutionContext::assertMainThread() {
        if (!_mainThread.isThisThread()) throw std::exception("not called in main thread");
    }

    ExecutionStatistics& ExecutionContext::statistics() {
        return _statistics;
    }

    void ExecutionContext::repositoriesNode(std::shared_ptr<RepositoriesNode> const& node) {
        if (_repositoriesNode != node) {
            if (_repositoriesNode != nullptr) {
                _nodes.removeIfPresent(_repositoriesNode);
            }
            _repositoriesNode = node;
            if (_repositoriesNode != nullptr) {
                _nodes.addIfAbsent(_repositoriesNode);
            }
        }
    }
    
    std::shared_ptr<RepositoriesNode> const& ExecutionContext::repositoriesNode() const {
        return _repositoriesNode;
    }

    std::shared_ptr<FileRepositoryNode> const& ExecutionContext::findRepository(std::string const& repoName) const {
        return 
            _repositoriesNode == nullptr 
            ? nullRepo 
            : _repositoriesNode->findRepository(repoName);
    }

    std::shared_ptr<FileRepositoryNode> const& ExecutionContext::findRepositoryContaining(std::filesystem::path const& path) const {
        return
            _repositoriesNode == nullptr
            ? nullRepo
            : _repositoriesNode->findRepositoryContaining(path);
    }

    std::map<std::string, std::shared_ptr<FileRepositoryNode>> const& ExecutionContext::repositories() const {
        static std::map<std::string, std::shared_ptr<FileRepositoryNode>> empty;
        return _repositoriesNode == nullptr ? empty : _repositoriesNode->repositories();
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

    void ExecutionContext::getBuildState(std::unordered_set<std::shared_ptr<IPersistable>>& buildState) {
        auto addToState = 
            Delegate<void, std::shared_ptr<Node> const&>::CreateLambda(
                [&](std::shared_ptr<Node> node) {buildState.insert(node); });

        _nodes.foreach(addToState);
        for (auto const& pair : repositories()) buildState.insert(pair.second);
    }

    void ExecutionContext::clearBuildState() {
        if (_repositoriesNode != nullptr) {
            _repositoriesNode->stopWatching();
            _repositoriesNode = nullptr;
        }
        _nodes.clear();
        _nodes.clearChangeSet();
    }
}