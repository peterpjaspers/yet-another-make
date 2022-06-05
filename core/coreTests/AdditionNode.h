#pragma once

#include "..\Node.h"
#include "NumberNode.h"

namespace YAMTest
{
	using namespace YAM;

	class AdditionNode : public Node
	{
	public:
		AdditionNode(ExecutionContext* context, std::filesystem::path const& name);

		void addOperand(NumberNode* operand);
		void clearOperands();

		NumberNode* sum();

		virtual bool supportsPrerequisites() const override;
		virtual void appendPrerequisites(std::vector<Node*>& prerequisites) const override;

		virtual bool supportsOutputs() const override;
		virtual void appendOutputs(std::vector<Node*>& outputs) const override;

		virtual bool supportsInputs() const override;
		virtual void appendInputs(std::vector<Node*>& inputs) const override;

		virtual bool pendingStartSelf() const override;

		virtual void startSelf() override;

	private:
		void execute();

		std::vector<NumberNode*> _operands;
		NumberNode _sum;
	};
}
