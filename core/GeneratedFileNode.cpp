#include "GeneratedFileNode.h"
#include "ExecutionContext.h"

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
}