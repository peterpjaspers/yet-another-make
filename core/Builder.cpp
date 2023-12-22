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
#include "DotGitDirectory.h"
#include "DotYamDirectory.h"
#include "AcyclicTrail.h"

#include <iostream>
#include <map>

#ifdef _DEBUG
#define ASSERT_MAIN_THREAD(context) (context).assertMainThread()
#else
#define ASSERT_MAIN_THREAD(context)
#endif

namespace
{
    using namespace YAM;

    void appendDirtyDirectoryNodes(
        std::shared_ptr<DirectoryNode> directory,
        std::vector<std::shared_ptr<Node>>& dirtyNodes
    ) {
        if (directory->state() != Node::State::Ok) {
            directory->setState(Node::State::Dirty);
            dirtyNodes.push_back(directory);
        }
        for (auto const& pair : directory->getContent()) {
            auto child = pair.second;
            auto sourceDir = dynamic_pointer_cast<DirectoryNode>(child);
            if (sourceDir != nullptr) {
                appendDirtyDirectoryNodes(sourceDir, dirtyNodes);
            } 
        }
    }

    void appendDirtyBuildFileParserNodes(
        std::shared_ptr<DirectoryNode> directory,
        std::vector<std::shared_ptr<Node>>& dirtyNodes
    ) {
        auto parser= directory->buildFileParserNode();
        if (parser != nullptr && parser->state() != Node::State::Ok) {
            parser->setState(Node::State::Dirty);
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
        if (compiler != nullptr && compiler->state() != Node::State::Ok) {
            compiler->setState(Node::State::Dirty);
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
            return (cmd != nullptr && cmd->state() != Node::State::Ok);
        });

    void appendDirtyCommands(NodeSet& nodes, std::vector<std::shared_ptr<Node>>& dirtyCommands) {
        nodes.find(includeNode, dirtyCommands);
        for (auto& cmd : dirtyCommands) cmd->setState(Node::State::Dirty);
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
        ASSERT_MAIN_THREAD(_context);
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
        if (_result->succeeded()) {
            if (_context.findRepository(".") == nullptr) {
                std::filesystem::path repoPath = yamDir.parent_path();
                auto repo = std::make_shared<FileRepository>(
                    ".",
                    repoPath,
                    &_context);
                _context.addRepository(repo);
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
                if (!generatedFileNode->deleteFile()) nFailures += 1;
            }
        });
        _context.nodes().foreach(cleanNode);
        _result->succeeded(nFailures==0);
    }

    bool Builder::containsCycles(std::vector<std::shared_ptr<Node>> const& buildFileParserNodes) const {
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

    // Called in main thread
    void Builder::_start() {
        std::vector<std::shared_ptr<Node>> dirtyDirs;        
        for (auto const& pair : _context.repositories()) {
            auto repo = pair.second;
            auto dirNode = repo->directoryNode();
            repo->consumeChanges();
            appendDirtyDirectoryNodes(dirNode, dirtyDirs);
        }
        if (dirtyDirs.empty()) {
            _handleDirectoriesCompletion(_dirtyDirectories.get());
        } else {
            _dirtyDirectories->group(dirtyDirs);
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
        } else if (containsCycles(_dirtyBuildFileParsers->group())) {
            _dirtyBuildFileCompilers->setState(Node::State::Failed);
            _handleBuildFileCompilersCompletion(_dirtyBuildFileCompilers.get());
        } else {
            std::vector<std::shared_ptr<Node>> dirtyBuildFileCompilers;
            for (auto const& pair : _context.repositories()) {
                auto repo = pair.second;
                auto dirNode = repo->directoryNode();
                appendDirtyBuildFileCompilerNodes(dirNode, dirtyBuildFileCompilers);
            }
            if (dirtyBuildFileCompilers.empty()) {
                _handleBuildFileCompilersCompletion(_dirtyBuildFileCompilers.get());
            } else {
                _dirtyBuildFileCompilers->group(dirtyBuildFileCompilers);
                _dirtyBuildFileCompilers->start();
            }
        }
    }

    void Builder::_handleBuildFileCompilersCompletion(Node* n) {
        if (n != _dirtyBuildFileCompilers.get()) throw std::exception("unexpected node");
        if (_dirtyBuildFileCompilers->state() != Node::State::Ok) {
            _dirtyCommands->setState(_dirtyBuildFileCompilers->state());
            _handleCommandsCompletion(_dirtyCommands.get());
        } else {
            std::vector<std::shared_ptr<Node>> dirtyCommands;
            appendDirtyCommands(_context.nodes(), dirtyCommands);
            if (dirtyCommands.empty()) {
                _handleCommandsCompletion(_dirtyCommands.get());
            } else {
                _dirtyCommands->group(dirtyCommands);
                _dirtyCommands->start();
            }
        }
    }

    // Called in main thread
    void Builder::_handleCommandsCompletion(Node* n) {
        if (n != _dirtyCommands.get()) throw std::exception("unexpected node");

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
        std::vector<std::shared_ptr<Node>> emptyNodes;
        _dirtyDirectories->group(emptyNodes);
        _dirtyBuildFileParsers->group(emptyNodes);
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
        ASSERT_MAIN_THREAD(_context);
        return _context.buildRequest() != nullptr;
    }

    void Builder::stop() {
        ASSERT_MAIN_THREAD(_context);
        _dirtyDirectories->cancel();
        _dirtyBuildFileParsers->cancel();
        _dirtyBuildFileCompilers->cancel();
        _dirtyCommands->cancel();
    }

    MulticastDelegate<std::shared_ptr<BuildResult>>& Builder::completor() {
        ASSERT_MAIN_THREAD(_context);
        return _completor;
    }
}


