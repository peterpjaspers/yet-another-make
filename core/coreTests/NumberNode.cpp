#include "NumberNode.h"
#include "../ExecutionContext.h"

namespace YAMTest
{
    using namespace YAM;

    NumberNode::NumberNode(
        ExecutionContext* context,
        std::filesystem::path const& name)
        : Node(context, name)
        , _number(rand())
        , _executionHash(rand()) {
    }

    int NumberNode::number() const { return _number; }

    void NumberNode::number(int newNumber) {
        if (_number != newNumber) {
            _number = newNumber;
            setState(Node::State::Dirty);
        }
    }

    // Inherited from Node
    bool NumberNode::supportsPrerequisites() const { return false; }

    void NumberNode::getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const {
        throw std::runtime_error("precondition violation");
    }

    bool NumberNode::supportsOutputs() const { return false; }

    void NumberNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        throw std::runtime_error("precondition violation");
    }

    bool NumberNode::supportsInputs() const { return false; }

    void NumberNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
        throw std::runtime_error("precondition violation");
    }

    XXH64_hash_t NumberNode::executionHash() const {
        return _executionHash;
    }

    XXH64_hash_t NumberNode::computeExecutionHash(int number) const {
        return number;
    }

    XXH64_hash_t NumberNode::computeExecutionHash() const {
        return computeExecutionHash(_number);
    }

    bool NumberNode::pendingStartSelf() const { return _executionHash != computeExecutionHash(); }

    void NumberNode::selfExecute(int newNumber, ExecutionResult* result) {
        result->_newState = Node::State::Ok;
        result->_number = newNumber;
        result->_executionHash = computeExecutionHash(newNumber);
    }

    void NumberNode::selfExecute() {
        auto result = std::make_shared<ExecutionResult>();
        selfExecute(_number, result.get());
        postSelfCompletion(result);
    }

    void NumberNode::commitSelfCompletion(SelfExecutionResult const* result) {
        if (result->_newState == Node::State::Ok) {
            auto r = reinterpret_cast<ExecutionResult const*>(result);
            _number = r->_number;
            _executionHash = r->_executionHash;
        }
    }
}

