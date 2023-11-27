#include "BuildFileProcessingNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "DirectoryNode.h"
#include "CommandNode.h"
#include "ExecutionContext.h"
#include "FileSystem.h"

#include <vector>


namespace
{
    using namespace YAM;

    uint32_t streamableTypeId = 0;

    void removeCommand(ExecutionContext* context, std::shared_ptr<CommandNode> cmd) {
        static std::vector<std::shared_ptr<GeneratedFileNode>> emptyOutputs;

        std::vector<std::shared_ptr<GeneratedFileNode>> outputs = cmd->outputs();
        cmd->outputs(emptyOutputs);
        for (auto const& output : outputs) {
            context->nodes().remove(output);
        }
        context->nodes().remove(cmd);
    }
}

namespace YAM
{

    BuildFileProcessingNode::BuildFileProcessingNode() {}

    BuildFileProcessingNode::BuildFileProcessingNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
        , _buildFileExecutor(std::make_shared<CommandNode>(context, "executor"))
    {
        _buildFileExecutor->addObserver(this);
    }

    BuildFileProcessingNode::~BuildFileProcessingNode() {
        _buildFileExecutor->removeObserver(this);
    }

    void BuildFileProcessingNode::buildFile(std::shared_ptr<SourceFileNode> const& newFile) {
        if (_buildFile != newFile) {
            if (_buildFile != nullptr) {
                for (auto const& cmd : _commands) removeCommand(context(), cmd);
                _commands.clear();
            }
            _buildFile = newFile;
            setState(Node::State::Dirty);
        }
    }

    std::shared_ptr<SourceFileNode> BuildFileProcessingNode::buildFile() const {
        return _buildFile;
    }

    void BuildFileProcessingNode::setupBuildFileExecutor() {
        _rulesFile = FileSystem::createUniqueDirectory() / "rules.txt";

        std::stringstream ss;
        ss << _buildFile->name().filename() << " > " << _rulesFile.string();
        std::string script = ss.str();
        _buildFileExecutor->script(script);

        auto wdir = _buildFile->name().parent_path();
        auto wdirNode = dynamic_pointer_cast<DirectoryNode>(context()->nodes().find(wdir));
        _buildFileExecutor->workingDirectory(wdirNode);
    }
    
    void BuildFileProcessingNode::teardownBuildFileExecutor() {
        _buildFileExecutor->workingDirectory(nullptr);
        _buildFileExecutor->script(std::string());
        std::filesystem::remove_all(_rulesFile.parent_path());
        _rulesFile = "";
    }

    void BuildFileProcessingNode::start() {
        Node::start();
        if (_buildFile == nullptr) {
            postCompletion(Node::State::Ok);
            return;
        }

        std::vector<Node*> requisites;
        // 1. execute buildfile => output to temporary file
        //    if (output hash not changed) => finish
        // 2. parse buildfile output => syntaxtree + buildfile dependencies
        // 3. delete temporary file
        // 4. execute buildfile dependencies => refd generated files
        // 5. evaluate syntax tree => updated set of command nodes

        setupBuildFileExecutor();
        requisites.push_back(_buildFileExecutor.get());
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State state) { handleBuildFileExecutorCompletion(state); }
        );
        startNodes(requisites, std::move(callback));
    }

    void BuildFileProcessingNode::handleBuildFileExecutorCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            Node::notifyCompletion(state);
        } else if (canceling()) {
            Node::notifyCompletion(Node::State::Canceled);
        } else {
            bool completed = true;
            Node::notifyCompletion(state);
        }
        teardownBuildFileExecutor();
    }

    void BuildFileProcessingNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        for (auto const& cmd : _commands) {
            outputs.push_back(cmd);
        }
    }

    void BuildFileProcessingNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
        for (auto const& pair : _inputs) {
            inputs.push_back(pair.second);
        }
    }

    void BuildFileProcessingNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t BuildFileProcessingNode::typeId() const {
        return streamableTypeId;
    }

    void BuildFileProcessingNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
    }

    void BuildFileProcessingNode::prepareDeserialize() {
        Node::prepareDeserialize();

    }
    void BuildFileProcessingNode::restore(void* context) {
        Node::restore(context);
    }

}