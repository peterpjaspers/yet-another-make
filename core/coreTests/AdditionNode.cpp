#include "AdditionNode.h"
#include "../ExecutionContext.h"

namespace YAMTest
{
	using namespace YAM;

	AdditionNode::AdditionNode(
		ExecutionContext* context,
		std::filesystem::path const& name)
		: Node(context, name)
		, _sum(context, "sumOf" / name)
		, _executionHash(rand()) {
		_sum.number(rand());
		_sum.addParent(this);
	}

	void AdditionNode::addOperand(NumberNode* operand) {
		_operands.push_back(operand);
		operand->addParent(this);
		setState(State::Dirty);
	}

	void AdditionNode::clearOperands(bool deleteOps) {
		if (!_operands.empty()) {
			for (auto op : _operands) {
				op->removeParent(this);
				if (deleteOps) delete op;
			}
			_operands.clear();
			setState(State::Dirty);
		}
	}

	NumberNode* AdditionNode::sum() { return &_sum; }

	bool AdditionNode::supportsPrerequisites() const { return true; }
	void AdditionNode::getPrerequisites(std::vector<Node*>& prerequisites) const {
		getInputs(prerequisites);
		getOutputs(prerequisites);
	}

	bool AdditionNode::supportsOutputs() const { return true; }
	void AdditionNode::getOutputs(std::vector<Node*>& outputs) const {
		outputs.push_back((Node*)(&_sum));
	}

	bool AdditionNode::supportsInputs() const { return true; }
	void AdditionNode::getInputs(std::vector<Node*>& inputs) const {
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
		hashes.push_back(_sum.executionHash());
		return XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
	}

	void AdditionNode::execute() {
		int sum = 0;
		for (auto op : _operands) {
			auto nr = dynamic_cast<NumberNode*>(op)->number();
			sum += nr;
		}
		_sum.setNumberRehashAndSetOk(sum);
		_executionHash = computeExecutionHash();
		postCompletion(Node::State::Ok);
	}
}
