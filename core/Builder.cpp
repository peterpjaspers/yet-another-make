#include "Builder.h"
#include "BuildRequest.h"
#include "BuildResult.h"
#include "CommandNode.h"
#include "SourceFileNode.h"
#include "SourceDirectoryNode.h"
#include "SourceFileRepository.h"
#include "GraphWalker.h"

#include <iostream>
#include <map>

namespace
{
    using namespace YAM;

    void appendDirtyFileAndDirectoryNodes(
        std::shared_ptr<Node> node,
        std::vector<std::shared_ptr<Node>>& dirtyNodes
    ) {
        if (node->state() == Node::State::Dirty) dirtyNodes.push_back(node);
        std::vector<std::shared_ptr<Node>> postRequisites; // DirectoryNodes and/or FileNodes
        node->getPostrequisites(postRequisites);
        for (auto n : postRequisites) appendDirtyFileAndDirectoryNodes(n, dirtyNodes);
    }

    bool isBuildFile(std::filesystem::path const& filePath) {
        static std::filesystem::path buildFileName("buildfile.yam");
        return filePath.filename() == buildFileName;
    }

    void appendBuildFileFileNodes(
        std::shared_ptr<Node> node,
        std::vector<std::shared_ptr<SourceFileNode>>& buildFiles
    ) {
        auto sourceFile = dynamic_pointer_cast<SourceFileNode>(node);
        if (sourceFile != nullptr && isBuildFile(sourceFile->name())) {
            buildFiles.push_back(sourceFile);
        }
        std::vector<std::shared_ptr<Node>> postRequisites;
        node->getPostrequisites(postRequisites);
        for (auto n : postRequisites) appendBuildFileFileNodes(n, buildFiles);
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
    Builder::Builder()
        : _scope(std::make_shared<CommandNode>(&_context, "__scope__"))
        , _result(nullptr)
    {}

    ExecutionContext* Builder::context() {
        return &_context;
    }

    void Builder::start(std::shared_ptr<BuildRequest> request) {
        if (running()) throw std::exception("cannot start a build while one is running");
        _result = std::make_shared<BuildResult>();
        if (request->requestType() == BuildRequest::Init) {
            _init(request->directory(), true);
            _notifyCompletion();
        } else if (request->requestType() == BuildRequest::Clean) {
            _clean(request);
        } else if (request->requestType() == BuildRequest::Build) {
            _init(request->directory(), false);
            _context.buildRequest(request);
            _context.mainThreadQueue().push(Delegate<void>::CreateLambda(([this]() { _start(); })));
        } else {
            throw std::exception("unknown build request type");
        }
    }
    void Builder::_init(std::filesystem::path directory, bool failIfAlreadyInitialized) {
        const std::string yam(".yam");
        std::filesystem::path yamDir(directory);
        while (!yamDir.empty() && !std::filesystem::exists(yamDir / yam)) {
            auto parent = yamDir.parent_path();
            yamDir = (parent == yamDir) ? "" : parent;
        }
        if (yamDir.empty()) {
            yamDir = directory / yam;
            std::filesystem::create_directory(yamDir);
            auto repo = std::make_shared<SourceFileRepository>(
                directory.filename().string(),
                directory,
                RegexSet({ 
                    RegexSet::matchDirectory("generated"),
                    RegexSet::matchDirectory(".yam")
                    }),
                &_context);
            _context.addRepository(repo);
            _result->succeeded(true);
        } else if (failIfAlreadyInitialized) {
            std::cerr
                << "YAM initialization failed" << std::endl
                << "Reason:" << ".yam directory already exists in parent directory: " << yamDir.parent_path().string()
                << std::endl;
            _result->succeeded(false);
        } else {
            _result->succeeded(true);
        }
    }

    void Builder::_clean(std::shared_ptr<BuildRequest> request) {
        throw std::exception("not implemented");
    }

    void Builder::_start() {
        _scope->setState(Node::State::Ok);
        std::vector<std::shared_ptr<Node>> dirtyDirsAndFiles;
        for (auto const& pair : _context.repositories()) {
            auto repo = dynamic_pointer_cast<SourceFileRepository>(pair.second);
            if (repo != nullptr) {
                auto dirNode = repo->directoryNode();
                repo->consumeChanges();
                appendDirtyFileAndDirectoryNodes(dirNode, dirtyDirsAndFiles);
            }
        }
        if (dirtyDirsAndFiles.empty()) {
            _handleRepoCompletion(_scope.get());
        } else {
            _scope->setInputProducers(dirtyDirsAndFiles);
            _scope->completor().AddRaw(this, &Builder::_handleRepoCompletion);
            _scope->start();
        }
    }

    void Builder::_handleRepoCompletion(Node* n) {
        if (n != _scope.get()) throw std::exception("unexpected node");
        _scope->completor().RemoveAll();
        _result->succeeded(_scope->state() == Node::State::Ok);
        _scope->setInputProducers(std::vector<std::shared_ptr<Node>>());
        if (!_result->succeeded()) {
            _notifyCompletion();
        } else {
            std::vector<std::shared_ptr<SourceFileNode>> buildFiles;
            for (auto const& pair : _context.repositories()) {
                auto repo = dynamic_pointer_cast<SourceFileRepository>(pair.second);
                if (repo != nullptr) {
                    auto dirNode = repo->directoryNode();
                    appendBuildFileFileNodes(dirNode, buildFiles);
                }
            }
            // TODO: update set of BuildFileParserNodes to be in sync with
            // buildFiles. Then request dirty BuildFileParserNodes to suspend 
            // the CommandNodes they created during previous execution.
            // Then execute the dirty BuildFileParserNodes and the Dirty command nodes
            // as per design in Miro board.
            std::vector<std::shared_ptr<Node>> dirtyCommands;
            appendDirtyCommands(_context.nodes(), dirtyCommands);
            if (dirtyCommands.empty()) {
                _result->succeeded(_scope->state() == Node::State::Ok);
                _notifyCompletion();
            } else {
                _scope->setInputProducers(dirtyCommands); // + dirtyBuildFileParsers
                _scope->completor().AddRaw(this, &Builder::_handleBuildCompletion);
                _scope->start();
            }
        }
    }

    void Builder::_handleBuildCompletion(Node* n) {
        if (n != _scope.get()) throw std::exception("unexpected node");
        _scope->completor().RemoveAll();
        _result->succeeded(_scope->state() == Node::State::Ok);
        _scope->setInputProducers(std::vector<std::shared_ptr<Node>>());
        _notifyCompletion();
    }

    void Builder::_notifyCompletion() {
        auto result = _result;
        _result = nullptr;
        _context.buildRequest(nullptr);
        _completor.Broadcast(result);
    }

    bool Builder::running() const {
        return _context.buildRequest() != nullptr;
    }

    void Builder::stop() {
        if (!running()) throw std::exception("cannot cancel a build while no one is running");
       _scope->cancel();
    }

    MulticastDelegate<std::shared_ptr<BuildResult>>& Builder::completor() {
        return _completor;
    }
}


