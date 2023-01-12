#pragma once

#include "../Node.h"
#include "../../xxHash/xxhash.h"

namespace YAMTest
{
	using namespace YAM;

	class NumberNode : public Node
	{
	public:
		NumberNode(ExecutionContext* context, std::filesystem::path const& name);

		int number() const;
		void number(int newNumber);

		void setNumberRehashAndSetOk(int newNumber);

		// Inherited from Node
		virtual bool supportsPrerequisites() const override;
		virtual void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const override;

		virtual bool supportsOutputs() const override;
		virtual void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;

		virtual bool supportsInputs() const override;
		virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

		void rehash();
		
		XXH64_hash_t executionHash() const;
		XXH64_hash_t computeExecutionHash() const;

	protected:
		virtual bool pendingStartSelf() const override;

		virtual void startSelf() override;

	private:
		void execute();

		int _number;
		XXH64_hash_t _executionHash;
	};
}
