#include "NumberNode.h"
#include "../ExecutionContext.h"

namespace YAMTest
{
	using namespace YAM;

	NumberNode::NumberNode(
		ExecutionContext* context,
		std::filesystem::path const& name)
		: Node(context, name)
		, _number(0) {
	}

	int NumberNode::number() const { return _number; }

	void NumberNode::number(int newNumber) {
		if (_number != newNumber) {
			_number = newNumber;
			setState(Node::State::Dirty);
		}
	}

	void NumberNode::setNumberRehashAndSetOk(int newNumber) {
		_number = newNumber;
		rehash();
		setState(Node::State::Ok);
	}

	// Inherited from Node
	bool NumberNode::supportsPrerequisites() const { return false; }

	void NumberNode::appendPrerequisites(std::vector<Node*>& prerequisites) const {
		throw std::runtime_error("precondition violation");
	}

	bool NumberNode::supportsOutputs() const { return false; }

	void NumberNode::appendOutputs(std::vector<Node*>& outputs) const {
		throw std::runtime_error("precondition violation");
	}

	bool NumberNode::supportsInputs() const { return false; }

	void NumberNode::appendInputs(std::vector<Node*>& inputs) const {
		throw std::runtime_error("precondition violation");
	}

	void NumberNode::rehash() {
		_executionHash = computeExecutionHash();
	}

	XXH64_hash_t NumberNode::executionHash() const {
		return _executionHash;
	}

	XXH64_hash_t NumberNode::computeExecutionHash() const {
		return _number;
	}

	bool NumberNode::pendingStartSelf() const { return _executionHash != computeExecutionHash(); }

	void NumberNode::startSelf() {
		auto d = Delegate<void>::CreateLambda([this]() { execute(); });
		_context->threadPoolQueue().push(std::move(d));
	}

	void NumberNode::execute() {
		rehash();
		postCompletion(Node::State::Ok);
	}
}

