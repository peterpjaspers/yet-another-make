#include "BuildFileProcessingNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "CommandNode.h"
#include "ExecutionContext.h"

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

    BuildFileProcessingNode::BuildFileProcessingNode() {

    }
    BuildFileProcessingNode::BuildFileProcessingNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
    { }

    void BuildFileProcessingNode::buildFile(std::shared_ptr<SourceFileNode> const& newFile) {
        if (_buildFile != newFile) {
            if (_buildFile != nullptr) {
                _inputs.erase(_buildFile->name());
                _buildFile->removeObserver(this);
                for (auto const& cmd : _commands) removeCommand(context(), cmd);
                _commands.clear();
            }
            _buildFile = newFile;
            if (_buildFile != nullptr) {
                _buildFile->addObserver(this);
                _inputs.insert({ _buildFile->name(), _buildFile });
            }
            setState(Node::State::Dirty);
        }
    }

    std::shared_ptr<SourceFileNode> BuildFileProcessingNode::buildFile() const {
        return _buildFile;
    }

    void BuildFileProcessingNode::start() {
        postCompletion(Node::State::Ok);
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