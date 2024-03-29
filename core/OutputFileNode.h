#pragma once
#include "FileNode.h"

namespace YAM
{
    class __declspec(dllexport) OutputFileNode : public FileNode
    {
    public:
        // Construct an output file node. Execution of the producer node
        // generates the output file.
        OutputFileNode(ExecutionContext* context, std::filesystem::path const& name, Node* producer);

        Node* producer() const;

    private:
        Node* _producer;
    };
}
