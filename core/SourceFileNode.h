#pragma once
#include "FileNode.h"

namespace  YAM
{
    class __declspec(dllexport) SourceFileNode : public FileNode
    {
    public:
        SourceFileNode() {} // needed for deserialization

        SourceFileNode(ExecutionContext* context, std::filesystem::path const& name);

        static void setStreamableType(uint32_t type);
    };
}
