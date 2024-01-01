#include "ExecutionContext.h"
#include "Node.h"
#include "FileRepository.h"
#include "BuildRequest.h"
#include "ConsoleLogBook.h"

namespace
{
    using namespace YAM;

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


    static std::shared_ptr<FileRepository> nullRepo;
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

    void ExecutionContext::assertMainThread() {
        if (!_mainThread.isThisThread()) throw std::exception("not called in main thread");
    }

    ExecutionStatistics& ExecutionContext::statistics() {
        return _statistics;
    }

    bool ExecutionContext::addRepository(std::shared_ptr<FileRepository> repo)
    {
        std::shared_ptr<FileRepository> duplicateRepo = findRepository(repo->name());
        bool duplicateName = nullptr != duplicateRepo;
        if (!duplicateName) {
            _repositories.insert({ repo->name(), repo });
        }
        return !duplicateName;
    }

    bool ExecutionContext::removeRepository(std::string const& repoName) {
        auto it = _repositories.find(repoName);
        bool found = it != _repositories.end();
        if (found) {
            auto srcRepo = it->second;
            srcRepo->clear();
            _repositories.erase(it);
        }
        return found;
    }

    std::shared_ptr<FileRepository> const& ExecutionContext::findRepository(std::string const& repoName) const {
        auto const it = _repositories.find(repoName);
        bool found = it != _repositories.end();
        if (found) {
            auto const& repo = it->second;
            return repo;
        }
        return nullRepo;
    }

    std::shared_ptr<FileRepository> const& ExecutionContext::findRepositoryContaining(std::filesystem::path const& path) const {
        for (auto const& pair : _repositories) {
            auto const& repo = pair.second;
            if (repo->lexicallyContains(path)) return repo;
        }
        return nullRepo;
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

    void ExecutionContext::getBuildState(std::unordered_set<std::shared_ptr<IPersistable>>& buildState) {
        auto addToState = 
            Delegate<void, std::shared_ptr<Node> const&>::CreateLambda(
                [&](std::shared_ptr<Node> node) {buildState.insert(node); });

        _nodes.foreach(addToState);
        for (auto const& pair : _repositories) { buildState.insert(pair.second); }
    }

    void ExecutionContext::clearBuildState() {
        _repositories.clear();
        _nodes.clear();
    }

    void ExecutionContext::computeStorageNeed(
        std::unordered_set<std::shared_ptr<IPersistable>> const& buildState,
        std::unordered_set<std::shared_ptr<IPersistable>> const& storedState,
        std::unordered_set<std::shared_ptr<IPersistable>>& toInsert,
        std::unordered_set<std::shared_ptr<IPersistable>>& toReplace,
        std::unordered_set<std::shared_ptr<IPersistable>>& toRemove
    ) {
        for (auto const& p : buildState) {
            if (storedState.contains(p)) {
                if (p->modified()) toReplace.insert(p);
            } else {
                p->modified(true);
                toInsert.insert(p);
            }
        }
        for (auto const& p : storedState) {
            if (!buildState.contains(p)) {
                toRemove.insert(p);
                // p may have been modified before it was remooved
                if (p->modified()) toReplace.erase(p);
            }
        }
    }
}