#include "GeneratedFileNode.h"
#include "ExecutionContext.h"

namespace YAM
{
    GeneratedFileNode::GeneratedFileNode(
        ExecutionContext* context,
        std::filesystem::path const& name,
        std::shared_ptr<Node> producer)
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

    std::shared_ptr<Node> GeneratedFileNode::producer() const {
        return _producer; 
    }
}