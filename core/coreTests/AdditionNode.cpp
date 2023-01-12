#include "AdditionNode.h"
#include "../ExecutionContext.h"

namespace YAMTest
{
	using namespace YAM;

	AdditionNode::AdditionNode(
		ExecutionContext* context,
		std::filesystem::path const& name)
		: Node(context, name)
		, _sum(std::make_shared<NumberNode>(context, "sumOf" / name))
		, _executionHash(rand()) {
		_sum->number(rand());
		_sum->addParent(this);
	}

	void AdditionNode::addOperand(std::shared_ptr<NumberNode> operand) {
		_operands.push_back(operand);
		operand->addParent(this);
		setState(State::Dirty);
	}

	void AdditionNode::clearOperands() {
		if (!_operands.empty()) {
			_operands.clear();
			setState(State::Dirty);
		}
	}

	std::shared_ptr<NumberNode> AdditionNode::sum() { return _sum; }

	bool AdditionNode::supportsPrerequisites() const { return true; }
	void AdditionNode::getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const {
		getInputs(prerequisites);
		getOutputs(prerequisites);
	}

	bool AdditionNode::supportsOutputs() const { return true; }
	void AdditionNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
		outputs.push_back(_sum);
	}

	bool AdditionNode::supportsInputs() const { return true; }
	void AdditionNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
		inputs.insert(inputs.end(), _operands.begin(), _operands.end());
	}

	bool AdditionNode::pendingStartSelf() const { return _executionHash != computeExecutionHash(); }

	void AdditionNode::startSelf() {
		auto d = Delegate<void>::CreateLambda([this]() { execute(); });
		_context->threadPoolQueue().push(std::move(d));
	}

	XXH64_hash_t AdditionNode::executionHash() const {
		return _executionHash;
	}

	XXH64_hash_t AdditionNode::computeExecutionHash() const {
		std::vector< XXH64_hash_t> hashes;
		for (auto node : _operands) {
			hashes.push_back(node->executionHash());
		}
		hashes.push_back(_sum->executionHash());
		return XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
	}

	void AdditionNode::execute() {
		int sum = 0;
		for (auto op : _operands) {
			auto nr = dynamic_cast<NumberNode*>(op.get())->number();
			sum += nr;
		}
		_sum->setNumberRehashAndSetOk(sum);
		_executionHash = computeExecutionHash();
		postCompletion(Node::State::Ok);
	}
}
