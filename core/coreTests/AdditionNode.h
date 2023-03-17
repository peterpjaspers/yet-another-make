#pragma once

#include "NumberNode.h"
#include "../Node.h"
#include "../../xxHash/xxhash.h"

namespace YAMTest
{
    using namespace YAM;

    class AdditionNode : public Node
    {
    public:
        AdditionNode(ExecutionContext* context, std::filesystem::path const& name);
        ~AdditionNode();

        void addOperand(std::shared_ptr<NumberNode> operand);
        void clearOperands();

        std::shared_ptr<NumberNode> sum();

        virtual bool supportsPrerequisites() const override;
        virtual void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const override;

        virtual bool supportsOutputs() const override;
        virtual void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;

        virtual bool supportsInputs() const override;
        virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

        virtual bool pendingStartSelf() const override;

        virtual void startSelf() override;

        XXH64_hash_t executionHash() const;
        XXH64_hash_t computeExecutionHash() const;

        // Inherited from IStreamable
        virtual uint32_t typeId() const { return 0; }
        virtual void stream(IStreamer* streamer) {}

    private:
        void execute();

        std::vector<std::shared_ptr<NumberNode>> _operands;
        std::shared_ptr<NumberNode> _sum;
        XXH64_hash_t _executionHash;
    };
}
