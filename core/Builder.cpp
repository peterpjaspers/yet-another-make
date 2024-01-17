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
#include "FileRepository.h"
#include "DotYamDirectory.h"
#include "AcyclicTrail.h"
#include "GroupCycleFinder.h"
#include "PersistentBuildState.h"

#include <iostream>
#include <map>

namespace
{
    using namespace YAM;

    std::vector<std::shared_ptr<Node>> emptyNodes;

    void resetNodeStates(NodeSet& nodes) {
        auto resetState = Delegate<void, std::shared_ptr<Node> const&>::CreateLambda(
            [](std::shared_ptr<Node> const& node) {
                if (node->state() != Node::State::Ok) node->setState(Node::State::Dirty);
            });
        nodes.foreach(resetState);
    }

    void appendDirtyDirectoryNodes(
        std::shared_ptr<DirectoryNode> directory, 
        std::map<std::filesystem::path, std::shared_ptr<DirectoryNode>>& dirtyDirs
    ) {
        if (directory->state() == Node::State::Dirty) {
            dirtyDirs.insert({ directory->name(), directory });
        }
        for (auto const& pair : directory->getContent()) {
            auto child = pair.second;
            auto dir = dynamic_pointer_cast<DirectoryNode>(child);
            if (dir != nullptr) {
                appendDirtyDirectoryNodes(dir, dirtyDirs);
            } 
        }
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

    void appendDirtyBuildFileParserNodes(
        std::shared_ptr<DirectoryNode> directory,
        std::vector<std::shared_ptr<Node>>& dirtyNodes
    ) {
        auto parser= directory->buildFileParserNode();
        if (parser != nullptr && parser->state() == Node::State::Dirty) {
            dirtyNodes.push_back(parser);
        }
        for (auto const& pair : directory->getContent()) {
            auto sourceDir = dynamic_pointer_cast<DirectoryNode>(pair.second);
            if (sourceDir != nullptr) {
                appendDirtyBuildFileParserNodes(sourceDir, dirtyNodes);
            }
        }
    }

    void appendDirtyBuildFileCompilerNodes(
        std::shared_ptr<DirectoryNode> directory,
        std::vector<std::shared_ptr<Node>>& dirtyNodes
    ) {
        auto compiler = directory->buildFileCompilerNode();
        if (compiler != nullptr && compiler->state() == Node::State::Dirty) {
            dirtyNodes.push_back(compiler);
        }
        for (auto const& pair : directory->getContent()) {
            auto sourceDir = dynamic_pointer_cast<DirectoryNode>(pair.second);
            if (sourceDir != nullptr) {
                appendDirtyBuildFileCompilerNodes(sourceDir, dirtyNodes);
            }
        }
    }

    bool isBuildFile(std::filesystem::path const& filePath) {
        static std::filesystem::path buildFileName("buildfile.yam");
        return filePath.filename() == buildFileName;
    }

    auto includeNode = Delegate<bool, std::shared_ptr<Node> const&>::CreateLambda(
        [](std::shared_ptr<Node> const& node) {
            auto cmd = dynamic_pointer_cast<CommandNode>(node);
            return (cmd != nullptr && cmd->state() == Node::State::Dirty);
        });

    void appendDirtyCommands(NodeSet& nodes, std::vector<std::shared_ptr<Node>>& dirtyCommands) {
        nodes.find(includeNode, dirtyCommands);
    }

    void logBuildFileCycle(
        std::shared_ptr<ILogBook> const& logBook, 
        std::shared_ptr<BuildFileParserNode> root, 
        std::list<const BuildFileParserNode*>const& trail
    ) {
        std::stringstream ss;
        ss << "Detected cycle in the buildfile dependency graph: " << std::endl;
        for (auto bfpn : trail) {
            ss << bfpn->absolutePath() << std::endl;
        }
        ss << root->absolutePath() << std::endl;
        ss << "Cycles in the buildfile dependency graph are not allowed." << std::endl;
        ss << "Refactor your buildfiles to remove the cycle." << std::endl;
        LogRecord error(LogRecord::Error, ss.str());
        logBook->add(error);
    }
}

namespace YAM
{
    // Called in any thread
    Builder::Builder()
        : _dirtyDirectories(std::make_shared<GroupNode>(&_context, "__dirtyDirectories__"))
        , _dirtyBuildFileParsers(std::make_shared<GroupNode>(&_context, "__dirtyBuildFileParsers__"))
        , _dirtyBuildFileCompilers(std::make_shared<GroupNode>(&_context, "__dirtyBuildFileCompilers__"))
        , _dirtyCommands(std::make_shared<GroupNode>(&_context, "__dirtyCommands__"))
        , _result(nullptr)
    {
        _dirtyDirectories->completor().AddRaw(this, &Builder::_handleDirectoriesCompletion);
        _dirtyBuildFileParsers->completor().AddRaw(this, &Builder::_handleBuildFileParsersCompletion);
        _dirtyBuildFileCompilers->completor().AddRaw(this, &Builder::_handleBuildFileCompilersCompletion);
        _dirtyCommands->completor().AddRaw(this, &Builder::_handleCommandsCompletion);
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
        if (request->requestType() == BuildRequest::Init) {
            _init(request->directory());
            _notifyCompletion();
        } else if (request->requestType() == BuildRequest::Clean) {
            _clean(request);
            _notifyCompletion();
        } else if (request->requestType() == BuildRequest::Build) {
            _init(request->directory());
            _start();
        } else {
            throw std::exception("unknown build request type");
        }
    }

    // Called in any thread
    void Builder::_init(std::filesystem::path directory) {
        std::filesystem::path yamDir = DotYamDirectory::initialize(directory, _context.logBook().get());
        _result->succeeded(!yamDir.empty());
        if (!_result->succeeded()) return;

        if (_buildState == nullptr) {
            if (_result->succeeded()) {
                _buildState = std::make_shared<PersistentBuildState>(yamDir, &_context);
                _buildState->retrieve();
                if (_context.findRepository(".") == nullptr) {
                    std::filesystem::path repoPath = yamDir.parent_path();
                    auto repo = std::make_shared<FileRepository>(
                        ".",
                        repoPath,
                        &_context);
                    _context.addRepository(repo);
                    _storeBuildState();
                }
            }
        }
    }

    // Called in main thread
    void Builder::_clean(std::shared_ptr<BuildRequest> request) {
        uint32_t nFailures = 0;
        auto cleanNode = Delegate<void, std::shared_ptr<Node> const&>::CreateLambda(
            [&nFailures](std::shared_ptr<Node> const& node) {
            auto generatedFileNode = dynamic_pointer_cast<GeneratedFileNode>(node);
        if (generatedFileNode != nullptr) {
            if (!generatedFileNode->deleteFile(true)) nFailures += 1;
        }
        });
        _context.nodes().foreach(cleanNode);
        _result->succeeded(nFailures == 0);
    }

    bool Builder::_containsBuildFileCycles(
        std::vector<std::shared_ptr<Node>> const& buildFileParserNodes
    ) const {
        if (buildFileParserNodes.empty()) return false;

        ILogBook& logBook = *(_context.logBook());
        LogRecord checking(LogRecord::Progress, "Checking for cycles in buildfile dependency graph.");
        logBook.add(checking);

        bool cycling = false;
        std::unordered_set<const Node*> visited;
        std::vector<BuildFileParserNode*> inCycle;
        for (auto const& node : buildFileParserNodes) {
            bool notVisited = visited.insert(node.get()).second;
            if (notVisited) {
                auto bfpNode = dynamic_pointer_cast<BuildFileParserNode>(node);
                AcyclicTrail<const BuildFileParserNode*> trail;
                bool notCycling = bfpNode->walkDependencies(trail);
                if (!notCycling) {
                    cycling = true;
                    inCycle.push_back(bfpNode.get());
                    logBuildFileCycle(bfpNode->context()->logBook(), bfpNode, trail.trail());
                }
            }
        }
        for (auto n : inCycle) {
            n->buildFile()->setState(Node::State::Dirty);
            n->setState(Node::State::Failed);
        }

        return cycling;
    }

    bool Builder::_containsGroupCycles(
        std::vector<std::shared_ptr<Node>> const& buildFileCompilerNodes
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
            compilers[0]->context()->logBook()->add(error);
        }        
        return cycling;
    }

    void Builder::_storeBuildState(bool logSave) {
        if (0 < _buildState->store()) {
            // TODO: find an efficient way to figure out whether objects will
            // be saved.
            if (logSave) {
                ILogBook& logBook = *(_context.logBook());
                LogRecord saving(LogRecord::Progress, "Saving buildstate");
                logBook.add(saving);
            }
        }
    }

    // Called in main thread
    void Builder::_start() {
        resetNodeStates(_context.nodes());
        std::map<std::filesystem::path, std::shared_ptr<DirectoryNode>> dirtyDirs;
        for (auto const& pair : _context.repositories()) {
            auto repo = pair.second;
            auto dirNode = repo->directoryNode();
            repo->consumeChanges();
            appendDirtyDirectoryNodes(dirNode, dirtyDirs);
        }
        std::vector<std::shared_ptr<Node>> prunedDirtyDirs = pruneDirtyDirectories(dirtyDirs);
        if (prunedDirtyDirs.empty()) {
            _handleDirectoriesCompletion(_dirtyDirectories.get());
        } else {
            ILogBook& logBook = *(_context.logBook());
            std::string msg = "Scanning repositor";
            (_context.repositories().size() > 1) ? msg.append("y") : msg.append("ies");
            LogRecord scanning(LogRecord::Progress, msg);
            logBook.add(scanning);

            _dirtyDirectories->group(prunedDirtyDirs);
            _dirtyDirectories->start();
        }
    }

    // Called in main thread
    void Builder::_handleDirectoriesCompletion(Node* n) {
        if (n != _dirtyDirectories.get()) throw std::exception("unexpected node");
        if (_dirtyDirectories->state() != Node::State::Ok) {
            _dirtyBuildFileParsers->setState(_dirtyDirectories->state());
            _handleBuildFileParsersCompletion(_dirtyBuildFileParsers.get());
        } else {
            if (!_dirtyDirectories->group().empty()) _storeBuildState();

            std::vector<std::shared_ptr<Node>> dirtyBuildFiles;
            for (auto const& pair : _context.repositories()) {
                auto repo = pair.second;
                auto dirNode = repo->directoryNode();
                appendDirtyBuildFileParserNodes(dirNode, dirtyBuildFiles);                
            }
            // TODO: find dirty build files
            // TODO: update set of BuildFileParserNodes to be in sync with
            // buildFiles. Then request dirty BuildFileParserNodes to suspend 
            // the CommandNodes they created during previous execution.
            // Then execute the dirty BuildFileParserNodes and the Dirty command nodes
            // as per design in Miro board.
            if (dirtyBuildFiles.empty()) {
                _handleBuildFileParsersCompletion(_dirtyBuildFileParsers.get());
            } else {
                ILogBook& logBook = *(_context.logBook());
                LogRecord parsing(LogRecord::Progress, "Parsing buildfiles");
                logBook.add(parsing);

                _dirtyBuildFileParsers->group(dirtyBuildFiles);
                _dirtyBuildFileParsers->start();
            }
        }
    }

    // Called in main thread
    void Builder::_handleBuildFileParsersCompletion(Node* n) {
        if (n != _dirtyBuildFileParsers.get()) throw std::exception("unexpected node");
        if (_dirtyBuildFileParsers->state() != Node::State::Ok) {
            _dirtyBuildFileCompilers->setState(_dirtyBuildFileParsers->state());
            _handleBuildFileCompilersCompletion(_dirtyBuildFileCompilers.get());
        } else if (_containsBuildFileCycles(_dirtyBuildFileParsers->group())) {
            _dirtyBuildFileCompilers->setState(Node::State::Failed);
            _handleBuildFileCompilersCompletion(_dirtyBuildFileCompilers.get());
        } else {
            if (!_dirtyBuildFileParsers->group().empty()) _storeBuildState();
            std::vector<std::shared_ptr<Node>> dirtyBuildFileCompilers;
            for (auto const& pair : _context.repositories()) {
                auto repo = pair.second;
                auto dirNode = repo->directoryNode();
                appendDirtyBuildFileCompilerNodes(dirNode, dirtyBuildFileCompilers);
            }
            if (dirtyBuildFileCompilers.empty()) {
                _handleBuildFileCompilersCompletion(_dirtyBuildFileCompilers.get());
            } else {
                ILogBook& logBook = *(_context.logBook());
                LogRecord compiling(LogRecord::Progress, "Compiling parsed buildfiles");
                logBook.add(compiling);

                _dirtyBuildFileCompilers->group(dirtyBuildFileCompilers);
                _dirtyBuildFileCompilers->start();
            }
        }
    }

    void Builder::_handleBuildFileCompilersCompletion(Node* n) {
        if (n != _dirtyBuildFileCompilers.get()) throw std::exception("unexpected node");
        if (_dirtyBuildFileCompilers->state() != Node::State::Ok) {
            _rollback();
            _dirtyCommands->setState(_dirtyBuildFileCompilers->state());
            _handleCommandsCompletion(_dirtyCommands.get());
        } else if (_containsGroupCycles(_dirtyBuildFileCompilers->group())) {
            _rollback();
            _dirtyCommands->setState(Node::State::Failed);
            _handleCommandsCompletion(_dirtyCommands.get());
        } else {
            std::vector<std::shared_ptr<Node>> dirtyCommands;
            appendDirtyCommands(_context.nodes(), dirtyCommands);
            if (
                !_dirtyBuildFileCompilers->group().empty()
                || !dirtyCommands.empty()
            ) {
                _storeBuildState();
            }
            if (dirtyCommands.empty()) {
                _handleCommandsCompletion(_dirtyCommands.get());
            } else {
                ILogBook& logBook = *(_context.logBook());
                LogRecord executing(LogRecord::Progress, "Executing commands");
                logBook.add(executing);

                _dirtyCommands->group(dirtyCommands);
                _dirtyCommands->start();
            }
        }
    }

    void Builder::_rollback() {
        _dirtyDirectories->group(emptyNodes);
        _dirtyBuildFileParsers->group(emptyNodes);
        _dirtyBuildFileCompilers->group(emptyNodes);
        _dirtyCommands->group(emptyNodes);
        _buildState->rollback();
    }

    // Called in main thread
    void Builder::_handleCommandsCompletion(Node* n) {
        if (n != _dirtyCommands.get()) throw std::exception("unexpected node");
        if (!_dirtyCommands->group().empty()) _storeBuildState();

        _result->succeeded(_dirtyCommands->state() == Node::State::Ok);
        _result->nDirectoryUpdates(_context.statistics().nDirectoryUpdates);
        _result->nNodesExecuted(_context.statistics().nSelfExecuted);
        _result->nNodesStarted(_context.statistics().nStarted);
        _result->nRehashedFiles(_context.statistics().nRehashedFiles);

        // Delay clearing the inputProducers of the _dirty* nodes to avoid
        // removing an observer on a node that is notifying.
        auto d = Delegate<void>::CreateLambda([this]() { _notifyCompletion(); });
        _context.mainThreadQueue().push(std::move(d));
    }

    // Called in main thread
    void Builder::_notifyCompletion() {
        _dirtyDirectories->group(emptyNodes);
        _dirtyBuildFileParsers->group(emptyNodes);
        _dirtyBuildFileCompilers->group(emptyNodes);
        _dirtyCommands->group(emptyNodes);
        _dirtyDirectories->setState(Node::State::Ok);
        _dirtyBuildFileParsers->setState(Node::State::Ok);
        _dirtyBuildFileCompilers->setState(Node::State::Ok);
        _dirtyCommands->setState(Node::State::Ok);

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


