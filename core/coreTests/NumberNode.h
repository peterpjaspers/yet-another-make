#pragma once

#include "..\Node.h"

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
		virtual void appendPrerequisites(std::vector<Node*>& prerequisites) const override;

		virtual bool supportsOutputs() const override;
		virtual void appendOutputs(std::vector<Node*>& outputs) const override;

		virtual bool supportsInputs() const override;
		virtual void appendInputs(std::vector<Node*>& inputs) const override;

		void rehash();

		virtual XXH64_hash_t computeExecutionHash() const override;

	protected:
		virtual bool pendingStartSelf() const override;

		virtual void startSelf() override;

	private:
		void execute();

		int _number;
	};
}
