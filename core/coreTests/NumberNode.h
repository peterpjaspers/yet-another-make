#pragma once

#include "../Node.h"
#include "../../xxHash/xxhash.h"

namespace YAMTest
{
    using namespace YAM;

    class NumberNode : public Node
    {
    public:
        struct ExecutionResult : Node::SelfExecutionResult {
            int _number;
            XXH64_hash_t _executionHash;
        };

        NumberNode(ExecutionContext* context, std::filesystem::path const& name);

        int number() const;
        void number(int newNumber);

        void selfExecute(int newNumber, ExecutionResult* result);

        // Inherited from Node
        virtual bool supportsPrerequisites() const override;
        virtual void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const override;

        virtual bool supportsOutputs() const override;
        virtual void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;

        virtual bool supportsInputs() const override;
        virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;
        
        XXH64_hash_t executionHash() const;
        XXH64_hash_t computeExecutionHash() const;
        XXH64_hash_t computeExecutionHash(int number) const;

        void commitSelfCompletion(SelfExecutionResult const* result) override;

        // Inherited from IStreamable
        virtual uint32_t typeId() const { return 0; }
        virtual void stream(IStreamer* streamer) {}

    protected:
        virtual bool pendingStartSelf() const override;

        void selfExecute() override;

    private:

        int _number;
        XXH64_hash_t _executionHash;
    };
}
