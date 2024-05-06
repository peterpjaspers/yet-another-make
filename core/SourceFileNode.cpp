#include "SourceFileNode.h"

namespace
{
    uint32_t streamableTypeId = 0;
}

namespace YAM
{
    SourceFileNode::SourceFileNode(
        ExecutionContext* context,
        std::filesystem::path const& name)
        : FileNode(context, name)
    {}

    void SourceFileNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t SourceFileNode::typeId() const {
        return streamableTypeId;
    }
}