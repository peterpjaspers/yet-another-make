#pragma once
#include "FileNode.h"

namespace YAM
{
    class __declspec(dllexport) GeneratedFileNode : public FileNode
    {
    public:
        // Construct an output file node. Execution of the producer node
        // generates the output file.
        GeneratedFileNode(ExecutionContext* context, std::filesystem::path const& name, Node* producer);

        virtual void setState(State newState) override;

        Node* producer() const;

    private:
        Node* _producer;
    };
}
