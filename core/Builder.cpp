#include "Builder.h"
#include "BuildRequest.h"
#include "BuildResult.h"
#include "CommandNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "SourceDirectoryNode.h"
#include "SourceFileRepository.h"
#include "DotGitDirectory.h"
#include "DotYamDirectory.h"

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

    void appendDirtyFileAndDirectoryNodes(
        std::shared_ptr<SourceDirectoryNode> directory,
        std::vector<std::shared_ptr<Node>>& dirtyNodes
    ) {
        if (directory->state() == Node::State::Dirty) dirtyNodes.push_back(directory);
        for (auto const& pair : directory->getContent()) {
            auto child = pair.second;
            auto sourceDir = dynamic_pointer_cast<SourceDirectoryNode>(child);
            if (sourceDir != nullptr) {
                appendDirtyFileAndDirectoryNodes(sourceDir, dirtyNodes);
            } else {
                if (child->state() == Node::State::Dirty) dirtyNodes.push_back(child);
            }
        }
    }

    bool isBuildFile(std::filesystem::path const& filePath) {
        static std::filesystem::path buildFileName("buildfile.yam");
        return filePath.filename() == buildFileName;
    }

    //TODO
    void appendDirtyBuildFileNodes(
        std::shared_ptr<SourceDirectoryNode> directory,
        std::vector<std::shared_ptr<Node>>& dirtyBuildFiles
    ) {
        //Take care: buildFileNode is NOT a source file node.
        //A BuildFileNode references one or more build files and
        //creates command nodes from the definitions in these files.
    }

    auto includeNode = Delegate<bool, std::shared_ptr<Node> const&>::CreateLambda(
        [](std::shared_ptr<Node> const& node) {
            auto cmd = dynamic_pointer_cast<CommandNode>(node);
            return (cmd != nullptr && cmd->state() == Node::State::Dirty);
        });

    void appendDirtyCommands(NodeSet& nodes, std::vector<std::shared_ptr<Node>>& dirtyCommands) {
        nodes.find(includeNode, dirtyCommands);
    }
}

namespace YAM
{
    // Called in any thread
    Builder::Builder(bool enableRepoMirroring)
        : _repoMirroringEnabled(enableRepoMirroring)
        , _dirtySources(std::make_shared<CommandNode>(&_context, "__dirtySources__"))
        , _dirtyBuildFiles(std::make_shared<CommandNode>(&_context, "__dirtyBuildFiles__"))
        , _dirtyCommands(std::make_shared<CommandNode>(&_context, "__dirtyCommands__"))
        , _result(nullptr)
    {
        _dirtySources->completor().AddRaw(this, &Builder::_handleSourcesCompletion);
        _dirtyBuildFiles->completor().AddRaw(this, &Builder::_handleBuildFilesCompletion);
        _dirtyCommands->completor().AddRaw(this, &Builder::_handleCommandsCompletion);
        _dirtySources->setState(Node::State::Ok);
        _dirtyBuildFiles->setState(Node::State::Ok);
        _dirtyCommands->setState(Node::State::Ok);
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
            //_context.mainThreadQueue().push(Delegate<void>::CreateLambda(([this]() { _start(); })));
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
            std::filesystem::path repoPath = yamDir.parent_path();
            if (_context.findRepository(repoPath.filename().string()) == nullptr) {
                auto repo = std::make_shared<SourceFileRepository>(
                    repoPath.filename().string(),
                    repoPath,
                    RegexSet({
                        RegexSet::matchDirectory("generated"),
                        RegexSet::matchDirectory(".yam"),
                        RegexSet::matchDirectory(".git"),
                        RegexSet::matchDirectory("x64"),
                        RegexSet::matchDirectory(".vs")
                        }),
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

    // Called in main thread
    void Builder::_start() {
        std::vector<std::shared_ptr<Node>> dirtyDirsAndFiles;
        if (_repoMirroringEnabled) {
            for (auto const& pair : _context.repositories()) {
                auto repo = dynamic_pointer_cast<SourceFileRepository>(pair.second);
                if (repo != nullptr) {
                    auto dirNode = repo->directoryNode();
                    repo->consumeChanges();
                    appendDirtyFileAndDirectoryNodes(dirNode, dirtyDirsAndFiles);
                }
            }
        }
        if (dirtyDirsAndFiles.empty()) {
            _handleSourcesCompletion(_dirtySources.get());
        } else {
            _dirtySources->inputProducers(dirtyDirsAndFiles);
            _dirtySources->start();
        }
    }

    // Called in main thread
    void Builder::_handleSourcesCompletion(Node* n) {
        if (n != _dirtySources.get()) throw std::exception("unexpected node");
        if (_dirtySources->state() != Node::State::Ok) {
            _dirtyBuildFiles->setState(_dirtySources->state());
            _handleBuildFilesCompletion(_dirtyBuildFiles.get());
        } else {
            _dirtySources->inputProducers(std::vector<std::shared_ptr<Node>>());
            _dirtySources->setState(Node::State::Ok);
            std::vector<std::shared_ptr<Node>> dirtyBuildFiles;
            // TODO: find dirty build files
            // TODO: update set of BuildFileParserNodes to be in sync with
            // buildFiles. Then request dirty BuildFileParserNodes to suspend 
            // the CommandNodes they created during previous execution.
            // Then execute the dirty BuildFileParserNodes and the Dirty command nodes
            // as per design in Miro board.
            if (dirtyBuildFiles.empty()) {
                _handleBuildFilesCompletion(_dirtyBuildFiles.get());
            } else {
                _dirtyBuildFiles->inputProducers(dirtyBuildFiles);
                _dirtyBuildFiles->start();
            }
        }
    }

    // Called in main thread
    void Builder::_handleBuildFilesCompletion(Node* n) {
        if (n != _dirtyBuildFiles.get()) throw std::exception("unexpected node");
        if (_dirtyBuildFiles->state() != Node::State::Ok) {
            _dirtyCommands->setState(_dirtyCommands->state());
            _handleCommandsCompletion(_dirtyCommands.get());
        } else {
            _dirtyCommands->inputProducers(std::vector<std::shared_ptr<Node>>());
            _dirtyCommands->setState(Node::State::Ok);
            std::vector<std::shared_ptr<Node>> dirtyCommands;
            appendDirtyCommands(_context.nodes(), dirtyCommands);
            if (dirtyCommands.empty()) {
                _handleCommandsCompletion(_dirtyCommands.get());
            } else {
                _dirtyCommands->inputProducers(dirtyCommands);
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

        _dirtyCommands->inputProducers(std::vector<std::shared_ptr<Node>>());
        _dirtyCommands->setState(Node::State::Ok);
        _notifyCompletion();
    }

    // Called in main thread
    void Builder::_notifyCompletion() {
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
        _dirtySources->cancel();
        _dirtyBuildFiles->cancel();
        _dirtyCommands->cancel();
    }

    MulticastDelegate<std::shared_ptr<BuildResult>>& Builder::completor() {
        ASSERT_MAIN_THREAD(_context);
        return _completor;
    }
}


