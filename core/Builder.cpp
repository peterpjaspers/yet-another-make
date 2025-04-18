#include "Builder.h"
#include "BuildRequest.h"
#include "BuildResult.h"
#include "CommandNode.h"
#include "GroupNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "DirectoryNode.h"
#include "BuildFileParserNode.h"
#include "BuildFileCompilerNode.h"
#include "RepositoriesNode.h"
#include "FileExecSpecsNode.h"
#include "FileRepositoryNode.h"
#include "DotYamDirectory.h"
#include "AcyclicTrail.h"
#include "BuildFileCycleFinder.h"
#include "GroupCycleFinder.h"
#include "BuildStateVersion.h"
#include "PersistentBuildState.h"
#include "RepositoryNameFile.h"
#include "Globber.h"
#include "Glob.h"
#include "BuildScopeFinder.h"
#include "PeriodicTimer.h"
#include "FileSystem.h"

#include <iostream>
#include <map>
#include <atomic>

#include "../accessMonitor/Monitor.h"

namespace
{
    using namespace YAM;

    std::vector<std::shared_ptr<Node>> emptyNodes;
    static std::string dirClass("DirectoryNode");
    static std::string parserClass("BuildFileParserNode");
    static std::string compilerClass("BuildFileCompilerNode");
    static std::string cmdClass("CommandNode");

    void resetFailedAndCanceledNodes(NodeSet& nodes) {
        std::vector<std::shared_ptr<Node>> toReset;
        for (auto const& pair : nodes.failedOrCanceledNodes()) {
            toReset.insert(toReset.end(), pair.second.begin(), pair.second.end());
        }
        for (auto const& node : toReset) node->setState(Node::State::Dirty);
    }

    template<typename T>
    void appendDirtyNodes(
        ExecutionContext* context,
        std::string const& nodeClass,
        std::vector<std::shared_ptr<T>>& dirtyNodes
    ) {
        auto const& dirtyNodesMap = context->nodes().dirtyNodes();
        auto it = dirtyNodesMap.find(nodeClass);
        if (it != dirtyNodesMap.end()) {
            for (auto const& node : it->second) {
                auto repoType = node->repository()->repoType();
                if (repoType != FileRepositoryNode::RepoType::Ignore) {
                    auto tnode = dynamic_pointer_cast<T>(node);
                    if (tnode == nullptr) throw std::runtime_error("not a node of type T");
                    if (tnode->state() != Node::State::Dirty) throw std::runtime_error("not a dirty node");
                    dirtyNodes.push_back(tnode);
                }
            }
        }
    }

    template<typename T>
    void appendDirtyNodesMap(
        ExecutionContext* context,
        std::string const& nodeClass,
        std::map<std::filesystem::path, std::shared_ptr<T>>& dirtyNodes
    ) {
        std::vector<std::shared_ptr<T>> dirtyNodesVec;
        appendDirtyNodes<T>(context, nodeClass, dirtyNodesVec);
        for (auto const& node : dirtyNodesVec) {
            dirtyNodes.insert({ node->name(), node });
        }
    }

    bool isBuildFile(std::filesystem::path const& filePath) {
        static std::filesystem::path buildFileName("buildfile.yam");
        return filePath.filename() == buildFileName;
    }

    // See class DirectoryNode for explanation why pruning is needed.
    std::vector<std::shared_ptr<Node>> pruneDirtyDirectories(
        std::map<std::filesystem::path, std::shared_ptr<DirectoryNode>> const& dirtyDirs
    ) {
        std::vector<std::shared_ptr<Node>> pruned;
        for (auto it = dirtyDirs.rbegin(); it != dirtyDirs.rend(); ++it) {
            auto dir = it->second;
            auto parent = dir->parent();
            if (parent == nullptr || parent->state() != Node::State::Dirty) {
                pruned.push_back(dir);
            }
        }
        return pruned;
    }
    void deleteLeftoverFiles(const std::filesystem::path& tempFolder, ILogBook* logBook) {
        if (!std::filesystem::exists(tempFolder)) {
            std::filesystem::create_directories(tempFolder);
        }
        else {
            auto dirIter = std::filesystem::directory_iterator(tempFolder);
            int fileCount = std::count_if(
                begin(dirIter),
                end(dirIter),
                [](auto& entry) { return true; }
            );
            if (fileCount > 0) {
                std::stringstream ss;
                ss << "Deleting " << fileCount << " directories from " << tempFolder;
                LogRecord progress(LogRecord::Progress, ss.str());
                logBook->add(progress);
            }
            std::error_code ec;
            for (auto const& entry : std::filesystem::directory_iterator(tempFolder)) {
                std::filesystem::remove_all(entry.path(), ec);
            }
        }
    }
    
    std::mutex _mutex;
    unsigned int _nBuilders = 0;
}

namespace YAM
{
    // Called in any thread
    Builder::Builder()
        : _dirtyConfigNodes(std::make_shared<GroupNode>(&_context, "__dirtyConfigNodes"))
        , _dirtyDirectories(std::make_shared<GroupNode>(&_context, "__dirtyDirectories__"))
        , _dirtyBuildFileParsers(std::make_shared<GroupNode>(&_context, "__dirtyBuildFileParsers__"))
        , _dirtyBuildFileCompilers(std::make_shared<GroupNode>(&_context, "__dirtyBuildFileCompilers__"))
        , _dirtyCommands(std::make_shared<GroupNode>(&_context, "__dirtyCommands__"))
        , _result(nullptr)
        , _periodicStorage(
            std::make_shared<PeriodicTimer>(
                std::chrono::seconds(10),
                _context.mainThreadQueue(),
                Delegate<void>::CreateLambda([this]() { _storeBuildState(); })))
    {
        _dirtyConfigNodes->completor().AddRaw(this, &Builder::_handleConfigNodesCompletion);
        _dirtyDirectories->completor().AddRaw(this, &Builder::_handleDirectoriesCompletion);
        _dirtyBuildFileParsers->completor().AddRaw(this, &Builder::_handleBuildFileParsersCompletion);
        _dirtyBuildFileCompilers->completor().AddRaw(this, &Builder::_handleBuildFileCompilersCompletion);
        _dirtyCommands->completor().AddRaw(this, &Builder::_handleCommandsCompletion);
        _dirtyConfigNodes->setState(Node::State::Ok);
        _dirtyDirectories->setState(Node::State::Ok);
        _dirtyBuildFileParsers->setState(Node::State::Ok);
        _dirtyBuildFileCompilers->setState(Node::State::Ok);
        _dirtyCommands->setState(Node::State::Ok);

        std::lock_guard<std::mutex> guard(_mutex);
        if (_nBuilders == 0) {
            AccessMonitor::enableMonitoring();
        }
        _nBuilders += 1;
    }

    Builder::~Builder() {
        std::lock_guard<std::mutex> guard(_mutex);
        if (_nBuilders == 1) {
            AccessMonitor::disableMonitoring();
        }
        _nBuilders -= 1;
    }

    // Called in any thread
    ExecutionContext* Builder::context() {
        return &_context;
    }

    // Called in main thread
    void Builder::start(std::shared_ptr<BuildRequest> request) {
        ASSERT_MAIN_THREAD(&_context);
        if (running()) throw std::exception("request handling already in progress");
        _context.statistics().reset();
        _context.buildRequest(request);
        _result = std::make_shared<BuildResult>();
        bool ok = _init(request);
        if (ok) {
            if (request->options()._clean) {
                _clean();
            } else {
                _start();
            }
        } else {
            _notifyCompletion(Node::State::Failed);
        }
    }

    void Builder::logRepoNotInitialized() {
        LogRecord e(LogRecord::Error, "Repository not initialized");
        _context.addToLogBook(e);
    }

    // Called in any thread
    bool Builder::_init(std::shared_ptr<BuildRequest> const& request) {
        std::filesystem::path repoDir = request->repoDirectory();
        RepositoryNameFile nameFile(repoDir);
        std::string repoName = nameFile.repoName();
        auto state = BuildResult::State::Ok;
        if (repoName.empty() || repoName != request->repoName()) {
            state = BuildResult::State::Failed;
        }
        _result->state(state);
        if (state != BuildResult::State::Ok) {
            logRepoNotInitialized();
            return false;
        }

        static unsigned int defaultThreads = std::thread::hardware_concurrency();
        static const unsigned int maxThreads = 5 * defaultThreads;
        uint32_t threads = request->options()._threads;
        if (threads == 0) threads = defaultThreads;
        else if (threads > maxThreads) threads = maxThreads;
        _context.threadPool().size(threads);

		deleteLeftoverFiles(FileSystem::yamTempFolder(), _context.logBook().get());

        if (_buildState == nullptr) {
            std::filesystem::path yamDir = repoDir / DotYamDirectory::yamName();
            std::filesystem::path buildStatePath = BuildStateVersion::select(yamDir, *(_context.logBook()));
            if (buildStatePath.empty()) {
                //incompatible file version
                _result->state(BuildResult::State::Failed);
            } else {
                std::filesystem::create_directories(buildStatePath.parent_path());
                _buildState = std::make_shared<PersistentBuildState>(buildStatePath, &_context);
                _buildState->retrieve();
                auto repositoriesNode = _context.repositoriesNode();
                if (repositoriesNode == nullptr) {
                    auto homeRepo = std::make_shared<FileRepositoryNode>(
                        &_context,
                        repoName,
                        repoDir,
                        FileRepositoryNode::RepoType::Build);
                    repositoriesNode = std::make_shared<RepositoriesNode>(&_context, homeRepo);
                    repositoriesNode->ignoreConfigFile(false);
                    _context.repositoriesNode(repositoriesNode);
                }
                for (auto const& pair : _context.nodes().nodesMap()) {
                    auto node = pair.second;
                    if (node->state() != Node::State::Deleted) {
                        node->setState(Node::State::Dirty);
                    } else {
                        throw std::runtime_error("Unexpected Node::State::Delete");
                    }
                }
                repositoriesNode->startWatching();
            }
        }
        return _result->state() == BuildResult::State::Ok;
    }

    // Called in main thread
    void Builder::_clean() {
        uint32_t nFailures = 0;
        ILogBook& logBook = *(_context.logBook());
        std::vector<std::shared_ptr<GeneratedFileNode>> genFiles;
        BuildScopeFinder finder(&_context, _context.buildRequest()->options());
        genFiles = finder.generatedFiles();
        for (auto const& file : genFiles) {
            if (!file->deleteFile(true, true)) nFailures += 1;
        }
        auto state = (nFailures == 0) ? BuildResult::State::Ok : BuildResult::State::Failed;
        _result->state(state);
        _notifyCompletion(nFailures == 0 ? Node::State::Ok : Node::State::Failed);
    }

    bool Builder::_containsBuildFileCycles(
        std::set<std::shared_ptr<Node>, Node::CompareName> const& buildFileParserNodes
    ) const {
        if (buildFileParserNodes.empty()) return false;

        ILogBook& logBook = *(_context.logBook());
        LogRecord checking(LogRecord::Progress, "Checking for cycles in buildfile dependency graph.");
        logBook.add(checking);

        std::vector<std::shared_ptr<BuildFileParserNode>> parsers;
        for (auto const& c : buildFileParserNodes) {
            parsers.push_back(dynamic_pointer_cast<BuildFileParserNode>(c));
        }
        BuildFileCycleFinder finder(parsers);
        bool cycling = !finder.cycles().empty();
        if (cycling) {
            std::string cycleLog = finder.cyclesToString();
            LogRecord error(LogRecord::Error, cycleLog);
            parsers[0]->context()->addToLogBook(error);
        }
        return cycling;
    }

    bool Builder::_containsGroupCycles(
        std::set<std::shared_ptr<Node>, Node::CompareName> const& buildFileCompilerNodes
    ) const {
        if (buildFileCompilerNodes.empty()) return false;

        ILogBook& logBook = *(_context.logBook());
        LogRecord checking(LogRecord::Progress, "Checking for cycles in group dependency graph.");
        logBook.add(checking);

        std::vector<std::shared_ptr<BuildFileCompilerNode>> compilers;
        for (auto const& c : buildFileCompilerNodes) {
            compilers.push_back(dynamic_pointer_cast<BuildFileCompilerNode>(c));
        }
        GroupCycleFinder finder(compilers);
        bool cycling = !finder.cycles().empty();
        if (cycling) {
            std::string cycleLog = finder.cyclesToString();
            LogRecord error(LogRecord::Error, cycleLog);
            compilers[0]->context()->addToLogBook(error);
        }
        return cycling;
    }

    void Builder::_storeBuildState() {
        auto start = std::chrono::system_clock::now();
        ILogBook& logBook = *(_context.logBook());
        std::size_t nStored = _buildState->store();
        if (logBook.mustLogAspect(LogRecord::BuildStateUpdate) && 0 < nStored) {
            auto duration = std::chrono::system_clock::now() - start;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
            std::stringstream ss;
            ss << "Updated " << nStored << " nodes in buildstate in " << ms << std::endl;
            LogRecord saving(LogRecord::Progress, ss.str());
            logBook.add(saving);
        }
    }

    // Called in main thread
    void Builder::_start() {
        _periodicStorage->resume();
        resetFailedAndCanceledNodes(_context.nodes());
        std::vector<std::shared_ptr<Node>> dirtyNodes;
        auto repositoriesNode = _context.repositoriesNode();
        repositoriesNode->homeRepository()->consumeChanges();
        if (repositoriesNode->state() == Node::State::Dirty) {
            if (repositoriesNode->parseAndUpdate()) {
                repositoriesNode->startWatching();
            } else {
                _postCompletion(Node::State::Failed);
                return;
            }
            dirtyNodes.push_back(repositoriesNode);
        }
        for (auto const& pair : repositoriesNode->repositories()) {
            auto& repo = pair.second;
            if (repo->repoType() != FileRepositoryNode::RepoType::Ignore) {
                repo->consumeChanges();
                auto fileExecSpecsNode = repo->fileExecSpecsNode();
                if (fileExecSpecsNode->state() == Node::State::Dirty) {
                    dirtyNodes.push_back(fileExecSpecsNode);
                }
            }
        }
        if (dirtyNodes.empty()) {
            _handleConfigNodesCompletion(_dirtyConfigNodes.get());
        } else {
            ILogBook& logBook = *(_context.logBook());
            LogRecord scanning(LogRecord::Progress, "Processing configuration changes");
            logBook.add(scanning);

            _dirtyConfigNodes->content(dirtyNodes);
            _dirtyConfigNodes->start(PriorityClass::VeryLow);
        }
    }

    // Called in main thread
    void Builder::_handleConfigNodesCompletion(Node* n) {
        if (n != _dirtyConfigNodes.get()) throw std::exception("unexpected node");
        if (_dirtyConfigNodes->state() != Node::State::Ok) {
            _postCompletion(Node::State::Failed);
        } else {
            std::map<std::filesystem::path, std::shared_ptr<DirectoryNode>> dirtyDirs;
            appendDirtyNodesMap<DirectoryNode>(&_context, dirClass, dirtyDirs);
            std::vector<std::shared_ptr<Node>> prunedDirtyDirs = pruneDirtyDirectories(dirtyDirs);
            if (prunedDirtyDirs.empty()) {
                _handleDirectoriesCompletion(_dirtyDirectories.get());
            } else {
                ILogBook& logBook = *(_context.logBook());
                LogRecord scanning(LogRecord::Progress, "Scanning filesystem");
                logBook.add(scanning);

                _dirtyDirectories->content(prunedDirtyDirs);
                _dirtyDirectories->start(PriorityClass::VeryLow);
            }
        }
    }

    // Called in main thread
    void Builder::_handleDirectoriesCompletion(Node* n) {
        if (n != _dirtyDirectories.get()) throw std::exception("unexpected node");
        if (_dirtyDirectories->state() != Node::State::Ok) {
            _postCompletion(Node::State::Failed);
        } else {
            std::vector<std::shared_ptr<Node>> dirtyBuildFiles;
            appendDirtyNodes<Node>(&_context, parserClass, dirtyBuildFiles);
            if (dirtyBuildFiles.empty()) {
                _handleBuildFileParsersCompletion(_dirtyBuildFileParsers.get());
            } else {
                ILogBook& logBook = *(_context.logBook());
                LogRecord parsing(LogRecord::Progress, "Parsing buildfiles");
                logBook.add(parsing);

                _dirtyBuildFileParsers->content(dirtyBuildFiles);
                _dirtyBuildFileParsers->start(PriorityClass::VeryLow);
            }
        }
    }

    // Called in main thread
    void Builder::_handleBuildFileParsersCompletion(Node* n) {
        if (n != _dirtyBuildFileParsers.get()) throw std::exception("unexpected node");
        if (_dirtyBuildFileParsers->state() != Node::State::Ok) {
            _postCompletion(Node::State::Failed);
        } else if (_containsBuildFileCycles(_dirtyBuildFileParsers->content())) {
            for (auto const& node : _dirtyBuildFileParsers->content()) {
                node->setState(Node::State::Failed);
            }
            _postCompletion(Node::State::Failed);
        } else {
            std::vector<std::shared_ptr<Node>> dirtyBuildFileCompilers;
            appendDirtyNodes<Node>(&_context, compilerClass, dirtyBuildFileCompilers);
            if (dirtyBuildFileCompilers.empty()) {
                _handleBuildFileCompilersCompletion(_dirtyBuildFileCompilers.get());
            } else {
                ILogBook& logBook = *(_context.logBook());
                LogRecord compiling(LogRecord::Progress, "Compiling parsed buildfiles");
                logBook.add(compiling);

                _dirtyBuildFileCompilers->content(dirtyBuildFileCompilers);
                _dirtyBuildFileCompilers->start(PriorityClass::VeryLow);
            }
        }
    }

    void Builder::_handleBuildFileCompilersCompletion(Node* n) {
        if (n != _dirtyBuildFileCompilers.get()) throw std::exception("unexpected node");
        if (_dirtyBuildFileCompilers->state() != Node::State::Ok) {
            _postCompletion(Node::State::Failed);
        } else if (_containsGroupCycles(_dirtyBuildFileCompilers->content())) {
            for (auto const& node : _dirtyBuildFileCompilers->content()) {
                node->setState(Node::State::Failed);
            }
            _postCompletion(Node::State::Failed);
        } else {
            std::vector<std::shared_ptr<Node>> dirtyCommands;
            bool finderError = false;
            try {
                BuildScopeFinder finder(&_context, _context.buildRequest()->options());
                dirtyCommands = finder.dirtyCommands();
            } catch (std::runtime_error e) {
                LogRecord error(LogRecord::Aspect::Error, e.what());
                _context.addToLogBook(error);
                finderError = true;
            }
            if (finderError) {
                _postCompletion(Node::State::Failed);
            } else {
                if (dirtyCommands.empty()) {
                    _handleCommandsCompletion(_dirtyCommands.get());
                } else {
                    ILogBook& logBook = *(_context.logBook());
                    LogRecord executing(LogRecord::Progress, "Executing commands");
                    logBook.add(executing);

                    std::vector<std::shared_ptr<Node>> dirtyNodes;
                    dirtyNodes.insert(dirtyNodes.end(), dirtyCommands.begin(), dirtyCommands.end());
                    _dirtyCommands->content(dirtyNodes);
                    _dirtyCommands->start(PriorityClass::VeryLow);
                }
            }
        }
    }

    // Called in main thread
    void Builder::_handleCommandsCompletion(Node* n) {
        if (n != _dirtyCommands.get()) throw std::exception("unexpected node");
        Node::State newState = _dirtyCommands->state();
        // Delay clearing the inputProducers of the _dirty* nodes to avoid
        // removing an observer on a node that is notifying.
        _postCompletion(newState);
    }

    void Builder::_postCompletion(Node::State resultState) {
        auto d = Delegate<void>::CreateLambda([this, resultState]() {
            _notifyCompletion(resultState);
        });
        _context.mainThreadQueue().push(std::move(d));
    }

    // Called in main thread
    void Builder::_notifyCompletion(Node::State resultState) {
        _periodicStorage->suspend();
        _storeBuildState();
        auto state = BuildResult::State::Unknown;
        if (resultState == Node::State::Ok) state = BuildResult::State::Ok;
        else if (resultState == Node::State::Canceled) state = BuildResult::State::Canceled;
        else if (resultState == Node::State::Failed) state = BuildResult::State::Failed;
        _result->state(state);
        _result->nDirectoryUpdates(_context.statistics().nDirectoryUpdates);
        _result->nNodesExecuted(_context.statistics().nSelfExecuted);
        _result->nNodesStarted(_context.statistics().nStarted);
        _result->nRehashedFiles(_context.statistics().nRehashedFiles);
        _dirtyConfigNodes->content(emptyNodes);
        _dirtyDirectories->content(emptyNodes);
        _dirtyBuildFileParsers->content(emptyNodes);
        _dirtyBuildFileCompilers->content(emptyNodes);
        _dirtyCommands->content(emptyNodes);
        _dirtyConfigNodes->setState(Node::State::Ok);
        _dirtyDirectories->setState(Node::State::Ok);
        _dirtyBuildFileParsers->setState(Node::State::Ok);
        _dirtyBuildFileCompilers->setState(Node::State::Ok);
        _dirtyCommands->setState(Node::State::Ok);

        //_buildState->logState(*(_context.logBook()));
        auto result = _result;
        _result = nullptr;
        _context.buildRequest(nullptr);
        _completor.Broadcast(result);
    }

    bool Builder::running() {
        ASSERT_MAIN_THREAD(&_context);
        return _context.buildRequest() != nullptr;
    }

    void Builder::stop() {
        ASSERT_MAIN_THREAD(&_context);
        _dirtyConfigNodes->cancel();
        _dirtyDirectories->cancel();
        _dirtyBuildFileParsers->cancel();
        _dirtyBuildFileCompilers->cancel();
        _dirtyCommands->cancel();
    }

    MulticastDelegate<std::shared_ptr<BuildResult>>& Builder::completor() {
        ASSERT_MAIN_THREAD(&_context);
        return _completor;
    }
}


