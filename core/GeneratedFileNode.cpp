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
        addAspect(FileAspect::entireFileAspect().name());
    }

    Node* GeneratedFileNode::producer() const { 
        return _producer; 
    }
}