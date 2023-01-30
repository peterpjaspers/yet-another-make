#include "SourceFileNode.h"

namespace YAM
{
	SourceFileNode::SourceFileNode(
        ExecutionContext* context,
        std::filesystem::path const& name)
        : FileNode(context, name)
    {}
}