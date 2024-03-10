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
#include "PersistentBuildState.h"
#include "RepositoryNameFile.h"
#include "Globber.h"
#include "Glob.h"
#include "BuildScopeFinder.h"

#include <iostream>
#include <map>

namespace
{
    using namespace YAM;

    std::vector<std::shared_ptr<Node>> emptyNodes;
    static std::string dirClass("DirectoryNode");
    static std::string parserClass("BuildFileParserNode");
    static std::string compilerClass("BuildFileCompilerNode");
    static std::string cmdClass("CommandNode");

    void resetNodeStates(NodeSet& nodes) {
        auto resetState = Delegate<void, std::shared_ptr<Node> const&>::CreateLambda(
            [](std::shared_ptr<Node> const& node) {
            if (
                node->state() != Node::State::Ok
                && node->state() != Node::State::Deleted
             ) {
                node->setState(Node::State::Dirty);
            }
        });
        nodes.foreach(resetState);
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
        ExecutionContext *context,
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
    }

    Builder::~Builder() {

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
        _result->succeeded(!repoName.empty() && repoName == request->repoName());
        if (!_result->succeeded()) {
            logRepoNotInitialized();
            return false;
        }
        
        if (_buildState == nullptr) {
            std::filesystem::path yamDir = repoDir / DotYamDirectory::yamName();
            _buildState = std::make_shared<PersistentBuildState>(yamDir, &_context);
            _buildState->retrieve();
            auto repositoriesNode = _context.repositoriesNode();
            if (repositoriesNode == nullptr) {
                auto homeRepo = std::make_shared<FileRepositoryNode>(
                    &_context,
                    repoName,
                    repoDir);
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
        return _result->succeeded();
    }

    // Called in main thread
    void Builder::_clean() {
        uint32_t nFailures = 0;
        std::vector<std::shared_ptr<GeneratedFileNode>> genFiles;
        BuildScopeFinder finder;
        genFiles = finder.findGeneratedFiles(&_context, _context.buildRequest()->options());
        for (auto const& file : genFiles) {
            if (!file->deleteFile(true)) nFailures += 1;
        }
        _result->succeeded(nFailures == 0);
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

    void Builder::_storeBuildState(bool logSave) {
        std::size_t nStored = _buildState->store();
        if (0 < nStored) {
            std::stringstream ss;
            ss << "Updated " << nStored << " nodes in buildstate." << std::endl;
            ILogBook& logBook = *(_context.logBook());
            LogRecord saving(LogRecord::Progress, ss.str());
            logBook.add(saving);
        }
    }

    // Called in main thread
    void Builder::_start() {
        resetNodeStates(_context.nodes());
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
            auto &repo = pair.second;
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
            _dirtyConfigNodes->start();
        }
    }

    // Called in main thread
    void Builder::_handleConfigNodesCompletion(Node* n) {
        _storeBuildState();
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
                _dirtyDirectories->start();
            }
        }
    }

    // Called in main thread
    void Builder::_handleDirectoriesCompletion(Node* n) {
        _storeBuildState();
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
                _dirtyBuildFileParsers->start();
            }
        }
    }

    // Called in main thread
    void Builder::_handleBuildFileParsersCompletion(Node* n) {
        _storeBuildState();
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
                _dirtyBuildFileCompilers->start();
            }
        }
    }

    void Builder::_handleBuildFileCompilersCompletion(Node* n) {
        _storeBuildState();
        if (n != _dirtyBuildFileCompilers.get()) throw std::exception("unexpected node");
        if (_dirtyBuildFileCompilers->state() != Node::State::Ok) {
            _postCompletion(Node::State::Failed);
         } else if (_containsGroupCycles(_dirtyBuildFileCompilers->content())) {
             for (auto const& node : _dirtyBuildFileCompilers->content()) {
                 node->setState(Node::State::Failed);
             }
             _postCompletion(Node::State::Failed);
        } else {
            std::vector<std::shared_ptr<CommandNode>> dirtyCommands;
            bool finderError = false;
            BuildScopeFinder finder;
            try {
                dirtyCommands = finder.findDirtyCommands(&_context, _context.buildRequest()->options());
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
                    _dirtyCommands->start();
                }
            }
        }
    }

    // Called in main thread
    void Builder::_handleCommandsCompletion(Node* n) {
        _storeBuildState();
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
        _result->succeeded(resultState == Node::State::Ok);
        if (resultState == Node::State::Ok) {
            _result->nDirectoryUpdates(_context.statistics().nDirectoryUpdates);
            _result->nNodesExecuted(_context.statistics().nSelfExecuted);
            _result->nNodesStarted(_context.statistics().nStarted);
            _result->nRehashedFiles(_context.statistics().nRehashedFiles);
        }

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


