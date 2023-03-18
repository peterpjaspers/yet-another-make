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
        Node* producer)
        : FileNode(context, name)
        , _producer(producer) 
    { 
    }

    void GeneratedFileNode::setState(State newState) {
        if (state() != newState) {
            Node::setState(newState);
            if (state() == Node::State::Dirty) {
                _producer->setState(Node::State::Dirty);
            }
        }
    }

    Node* GeneratedFileNode::producer() const {
        return _producer;
    }

    bool GeneratedFileNode::deleteFile() {
        std::error_code ec;
        bool deleted = std::filesystem::remove(name(), ec);
        if (deleted) {
            setState(Node::State::Dirty);
        }
        return !ec;
    }

    void GeneratedFileNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t GeneratedFileNode::typeId() const {
        return streamableTypeId;
    }

    void GeneratedFileNode::producer(Node* producer) {
        _producer = producer;
    }
}