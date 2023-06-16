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
        , _executionHash(rand()) 
    {
        _sum->number(rand());
        _sum->addDependant(this);
    }

    AdditionNode::~AdditionNode() {
        clearOperands();
        _sum->removeDependant(this);
    }

    void AdditionNode::addOperand(std::shared_ptr<NumberNode> operand) {
        _operands.push_back(operand);
        operand->addDependant(this);
        setState(State::Dirty);
    }

    void AdditionNode::clearOperands() {
        if (!_operands.empty()) {
            for (auto op : _operands) op->removeDependant(this);
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

    XXH64_hash_t AdditionNode::executionHash() const {
        return _executionHash;
    }

    XXH64_hash_t AdditionNode::computeExecutionHash(XXH64_hash_t sumHash) const {
        std::vector<XXH64_hash_t> hashes;
        for (auto node : _operands) {
            hashes.push_back(node->executionHash());
        }
        hashes.push_back(sumHash);
        return XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
    }

    XXH64_hash_t AdditionNode::computeExecutionHash() const {
        return computeExecutionHash(_sum->executionHash());
    }

    void AdditionNode::selfExecute(ExecutionResult* result) {
        result->_sumResult = std::make_shared<NumberNode::ExecutionResult>();
        int sum = 0;
        for (auto op : _operands) {
            auto nr = dynamic_cast<NumberNode*>(op.get())->number();
            sum += nr;
        }
        _sum->selfExecute(sum, result->_sumResult.get());
        result->_executionHash = computeExecutionHash(result->_sumResult->_executionHash);
        result->_newState = Node::State::Ok;
    }

    void AdditionNode::selfExecute() {
        auto result = std::make_shared<ExecutionResult>();
        selfExecute(result.get());
        postSelfCompletion(result);
    }

    void AdditionNode::commitSelfCompletion(SelfExecutionResult const* result) {
        if (result->_newState == Node::State::Ok) {
            auto r = reinterpret_cast<ExecutionResult const*>(result);
            _sum->commitSelfCompletion(r->_sumResult.get());
            _executionHash = r->_executionHash;
        }

    }
}
