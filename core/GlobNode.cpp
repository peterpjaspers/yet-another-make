#include "GlobNode.h"
#include "Globber.h"
#include "DirectoryNode.h"
#include "ExecutionContext.h"

#include <set>

namespace YAM
{
    GlobNode::GlobNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
        , _inputsHash(rand())
        , _executionHash(rand())
    {}

    // The path pattern is relative to the base directory.
    void GlobNode::baseDirectory(std::shared_ptr<DirectoryNode> const& newBaseDir) {
        if (_baseDir != newBaseDir) {
            _baseDir = newBaseDir;
            setState(Node::State::Dirty);
        }
    }

    GlobNode::~GlobNode() {
        for (auto const& dir : _inputDirs) dir->removeObserver(this);
    }

    void GlobNode::pattern(std::filesystem::path const& newPattern) {
        if (_pattern != newPattern) {
            _pattern = newPattern;
            setState(Node::State::Dirty);
        }
    }

    void GlobNode::start() {
        Node::start();
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State state) { handleInputDirsCompletion(state); }
        );
        std::vector<Node*> inputs;
        for (auto const& n : _inputDirs) inputs.push_back(n.get());
        Node::startNodes(inputs, callback);
    }

    void GlobNode::handleInputDirsCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            notifyCompletion(state);
        } else if (canceling()) {
            notifyCompletion(Node::State::Canceled);
        } else if (_inputsHash == computeInputsHash()) {
            notifyCompletion(Node::State::Ok);
        } else {
            context()->statistics().registerSelfExecuted(this);
            auto d = Delegate<void>::CreateLambda(
                [this]() { executeGlob(); }
            );
            context()->threadPoolQueue().push(std::move(d));
        }
    }

    void GlobNode::initialize() {
        std::pair<std::shared_ptr<Globber>, std::string> result = execute();
        handleGlobCompletion(result.first, result.second);
        Node::State newState = result.second.empty() ? Node::State::Ok : Node::State::Failed;
        setState(newState);
    }

    std::pair<std::shared_ptr<Globber>, std::string> GlobNode::execute() {
        auto globber = std::make_shared<Globber>(_baseDir, _pattern, false);
        std::string error;
        try {
            globber->execute();
        } catch (std::runtime_error e) {
            error = e.what();
        } catch (...) {
            error = "unknown glob error";
        }
        return { globber, error };
    }

    void GlobNode::executeGlob() {
        auto result = execute();
        auto d = Delegate<void>::CreateLambda([this, result]() {
            Node::State newState = result.second.empty() ? Node::State::Ok : Node::State::Failed;
            handleGlobCompletion(result.first, result.second);
            notifyCompletion(newState); 
        });
        context()->mainThreadQueue().push(std::move(d));
    }

    void GlobNode::handleGlobCompletion(std::shared_ptr<Globber> const& globber, std::string const& error) {
        if (error.empty()) {
            _matches = globber->matches();
            for (auto const& dir : _inputDirs) dir->removeObserver(this);
            _inputDirs = globber->inputDirs();
            for (auto const& dir : _inputDirs) dir->addObserver(this);
            _executionHash = computeExecutionHash();
            _inputsHash = computeInputsHash();
        } else {
            LogRecord errorRecord(LogRecord::Aspect::Error, error);
            context()->logBook()->add(errorRecord);
        }
    }

    XXH64_hash_t GlobNode::computeExecutionHash() const {
        std::vector<XXH64_hash_t> hashes;
        hashes.push_back(XXH64_string(_baseDir->name().string()));
        hashes.push_back(XXH64_string(_pattern.string()));
        for (auto const& m : _matches) hashes.push_back(XXH64_string(m->name().string()));
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    XXH64_hash_t GlobNode::computeInputsHash() const {
        std::vector<XXH64_hash_t> hashes;
        for (auto const& dir : _inputDirs) hashes.push_back(dir->executionHash());
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

}