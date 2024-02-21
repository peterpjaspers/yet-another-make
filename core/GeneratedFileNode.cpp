#include "GeneratedFileNode.h"
#include "ExecutionContext.h"
#include "CommandNode.h"
#include "IStreamer.h"

namespace
{
    uint32_t streamableTypeId = 0;
}

namespace YAM
{
    GeneratedFileNode::GeneratedFileNode(
        ExecutionContext* context,
        std::filesystem::path const& name,
        std::shared_ptr<CommandNode> const& producer)
        : FileNode(context, name)
        , _producer(producer) 
    {}

    std::shared_ptr<CommandNode> GeneratedFileNode::producer() const {
        return _producer;
    }

    bool GeneratedFileNode::deleteFile(bool setDirty) {
        std::error_code ok;
        std::error_code ec;
        auto absPath = absolutePath();
        bool deleted = std::filesystem::remove(absPath, ec);
        if (deleted && setDirty) {
            setState(Node::State::Dirty);
        }
        return !std::filesystem::exists(absPath);
    }

    void GeneratedFileNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t GeneratedFileNode::typeId() const {
        return streamableTypeId;
    }

    void GeneratedFileNode::stream(IStreamer* streamer) {
        FileNode::stream(streamer); 
        streamer->stream(_producer);
    }
}