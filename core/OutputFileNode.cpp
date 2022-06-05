#include "OutputFileNode.h"
#include "ExecutionContext.h"

namespace YAM
{
    OutputFileNode::OutputFileNode(
        ExecutionContext* context,
        std::filesystem::path const& name,
        Node* producer)
        : FileNode(context, name)
        , _producer(producer) 
    { }

    Node* OutputFileNode::producer() const { return _producer; }
}