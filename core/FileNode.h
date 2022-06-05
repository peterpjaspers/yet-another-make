#pragma once
#include "Node.h"

namespace YAM
{
    class __declspec(dllexport) FileNode : public Node
    {
    public:
        FileNode(ExecutionContext* context, std::filesystem::path name);

        // Inherited via Node
        virtual bool supportsPrerequisites() const override;
        virtual void appendPrerequisites(std::vector<Node*>& prerequisites) const override;

        virtual bool supportsOutputs() const override;
        virtual void appendOutputs(std::vector<Node*>& outputs) const override;

        virtual bool supportsInputs() const override;
        virtual void appendInputs(std::vector<Node*>& inputs) const override;

        virtual XXH64_hash_t computeExecutionHash() const override;

        std::chrono::time_point<std::chrono::utc_clock> lastWriteTime() const;
        
        void rehash();

    protected:
        // Inherited via Node
        virtual bool pendingStartSelf() const override;
        virtual void startSelf() override;

    private:
        void execute();

        // file modification time captured just before computing Node::_executionHash
        std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;
    };
}

