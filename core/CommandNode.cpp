#include "CommandNode.h"
#include "SourceFileNode.h"
#include "OutputFileNode.h"
#include "ExecutionContext.h"

#include <iostream>
#include <boost/process.hpp>

namespace YAM
{
	using namespace boost::process;

	CommandNode::CommandNode(
		ExecutionContext* context,
		std::filesystem::path const& name)
		: Node(context, name)
		, _scriptHash(rand())
	{}

	void CommandNode::setInputAspects(FileAspectSet const& newInputAspects) {
		_inputAspects = newInputAspects;
		// TODO: must state be set to Dirty? Or only if aspects change?
	}

	void CommandNode::setOutputs(std::vector<OutputFileNode*> const & newOutputs) {
		if (_outputs != newOutputs) {
			// TODO: ownership? What if outputs are still referenced?
			for (auto i : _outputs) i->removeParent(this);
			_outputs = newOutputs;
			for (auto i : _outputs) i->addParent(this);
			setState(State::Dirty);
		}
	}

	void CommandNode::setScript(std::string const& newScript) {
		if (newScript != _script) {
			_script = newScript;
			auto newHash = XXH64_string(_script);
			if (_scriptHash != newHash) {
				_scriptHash = newHash;
				setState(State::Dirty);
			}
		}
	}

	void CommandNode::setInputProducers(std::vector<Node*> const& newInputProducers) {
		if (_inputProducers != newInputProducers) {
			for (auto i : _inputProducers) i->removeParent(this);
			_inputProducers = newInputProducers;
			for (auto i : _inputProducers) i->addParent(this);
			setState(State::Dirty);
		}
	}

	void CommandNode::appendInputProducers(std::vector<Node*>& producers) const {
		producers.insert(producers.end(), _inputProducers.begin(), _inputProducers.end());
	}

	bool CommandNode::supportsPrerequisites() const { return true; }
	void CommandNode::appendPrerequisites(std::vector<Node*>& prerequisites) const {
		prerequisites.insert(prerequisites.end(), _inputProducers.begin(), _inputProducers.end());
		appendSourceInputs(prerequisites);
		appendOutputs(prerequisites);
	}

	bool CommandNode::supportsOutputs() const { return true; }
	void CommandNode::appendOutputs(std::vector<Node*>& outputs) const {
		outputs.insert(outputs.end(), _outputs.begin(), _outputs.end());
	}

	bool CommandNode::supportsInputs() const { return true; }
	void CommandNode::appendInputs(std::vector<Node*>& inputs) const {
		inputs.insert(inputs.end(), _inputs.begin(), _inputs.end());
	}

	bool CommandNode::pendingStartSelf() const {
		return _executionHash != computeExecutionHash(); 
	}

	XXH64_hash_t CommandNode::computeExecutionHash() const {
		static std::string entireFile("entireFile");
		std::vector<XXH64_hash_t> hashes;
		hashes.push_back(_scriptHash);
		for (auto node : _outputs) hashes.push_back(node->hashOf(entireFile));
		for (auto node : _inputs) {
			std::string const& aspectName = _inputAspects.findApplicableAspectName(node->name());
			hashes.push_back(node->hashOf(aspectName));
		}
		return XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
	}

	void CommandNode::startSelf() {
		auto d = Delegate<void>::CreateLambda([this]() { execute(); });
		_context->threadPoolQueue().push(std::move(d));
	}

	void CommandNode::appendSourceInputs(std::vector<Node*> & sourceInputs) const {
		for (auto i : _inputs) {
			auto file = dynamic_cast<SourceFileNode*>(i);
			if (file != nullptr) sourceInputs.push_back(file);
		}
	}

	// Update hashes of output files
	// Note that it is safe to do this in threadpool context during
	// this node's self-execution because the main thread will not 
	// access these outputs while this node is executing.
	void CommandNode::rehashOutputs() {
		for (auto ofile : _outputs) {
			// output files are part of this node's prerequisites and were
			// hence executed, and moved to state Ok, before the start of 
			// this node's self-execution. 
			if (ofile->state() != State::Ok) throw std::runtime_error("state error");
			ofile->rehash();
		}
	}

	// TODO: detected in/output files, handle cancelation, log script output
	//
	Node::State CommandNode::executeScript() {
		ipstream stdoutOfScript;
		group g;
		child c(_script, g, std_out > stdoutOfScript);

		std::vector<std::string> data;
		std::string line;
		while (c.running() && std::getline(stdoutOfScript, line)) {
			data.push_back(line);
			std::cout << line << std::endl;
		}
		g.wait();
		c.wait();
		return (c.exit_code() == 0) ? State::Ok : State::Failed;
	}

	void CommandNode::execute() {

		State newState = executeScript();

		rehashOutputs();
		_executionHash = computeExecutionHash();

		postCompletion(newState);
	}
}
