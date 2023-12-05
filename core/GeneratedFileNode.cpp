#include "GeneratedFileNode.h"
#include "ExecutionContext.h"

namespace
{
    uint32_t streamableTypeId = 0;
}

namespace YAM
{
    GeneratedFileNode::GeneratedFileNode(
        ExecutionContext* context,
        std::filesystem::path const& name,
        CommandNode* producer)
        : FileNode(context, name)
        , _producer(producer) 
    {}

    CommandNode* GeneratedFileNode::producer() const {
        return _producer;
    }

    bool GeneratedFileNode::deleteFile() {
        std::error_code ok;
        std::error_code ec;
        bool deleted = std::filesystem::remove(name(), ec);
        if (deleted) {
            setState(Node::State::Dirty);
        }
        return ec != ok;
    }

    void GeneratedFileNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t GeneratedFileNode::typeId() const {
        return streamableTypeId;
    }

    void GeneratedFileNode::producer(CommandNode* producer) {
        _producer = producer;
    }
}