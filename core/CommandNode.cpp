#include "CommandNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "ExecutionContext.h"
#include "MonitoredProcess.h"
#include "FileAspectSet.h"
#include "FileSystem.h"

#include <iostream>
#include <fstream>
#include <boost/process.hpp>

namespace
{
	std::string cmdExe = boost::process::search_path("cmd").string();
}

namespace YAM
{
	using namespace boost::process;

	CommandNode::CommandNode(
		ExecutionContext* context,
		std::filesystem::path const& name)
		: Node(context, name)
		, _inputAspectsName(FileAspectSet::entireFileSet().name())
		, _scriptHash(rand())
		, _executionHash(rand())
	{}

	CommandNode::~CommandNode() {
		setOutputs(std::vector<std::shared_ptr<GeneratedFileNode>>());
		setInputProducers(std::vector<std::shared_ptr<Node>>());
	}

	void CommandNode::setState(State newState) {
		if (state() != newState) {
			Node::setState(newState);
			if (state() == Node::State::Dirty) {
				for (auto const& n : _outputs) {
					n->setState(Node::State::Dirty);
				}
			}
		}
	}

	void CommandNode::setInputAspectsName(std::string const& newName) {
		if (_inputAspectsName != newName) {
			_inputAspectsName = newName;
			setState(State::Dirty);
		}
	}

	void CommandNode::setOutputs(std::vector<std::shared_ptr<GeneratedFileNode>> const & newOutputs) {
		if (_outputs != newOutputs) {
			for (auto i : _outputs) i->removePreParent(this);
			_outputs = newOutputs;
			for (auto i : _outputs) i->addPreParent(this);
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

	void CommandNode::setInputProducers(std::vector<std::shared_ptr<Node>> const& newInputProducers) {
		if (_inputProducers != newInputProducers) {
			for (auto i : _inputProducers) i->removePreParent(this);
			_inputProducers = newInputProducers;
			for (auto i : _inputProducers) i->addPreParent(this);
			setState(State::Dirty);
		}
	}

	void CommandNode::getInputProducers(std::vector<std::shared_ptr<Node>>& producers) const {
		producers.insert(producers.end(), _inputProducers.begin(), _inputProducers.end());
	}

	bool CommandNode::supportsPrerequisites() const { return true; }
	void CommandNode::getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const {
		prerequisites.insert(prerequisites.end(), _inputProducers.begin(), _inputProducers.end());
		getSourceInputs(prerequisites);
		getOutputs(prerequisites);
	}

	bool CommandNode::supportsOutputs() const { return true; }
	void CommandNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
		outputs.insert(outputs.end(), _outputs.begin(), _outputs.end());
	}

	bool CommandNode::supportsInputs() const { return true; }
	void CommandNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
		inputs.insert(inputs.end(), _inputs.begin(), _inputs.end());
	}

	bool CommandNode::pendingStartSelf() const {
		bool pending = _executionHash != computeExecutionHash();
		return pending;
	}

	XXH64_hash_t CommandNode::computeExecutionHash() const {
		std::vector<XXH64_hash_t> hashes;
		hashes.push_back(_scriptHash);
		for (auto node : _outputs) hashes.push_back(node->hashOf(FileAspect::entireFileAspect().name()));
		FileAspectSet inputAspects = context()->findFileAspectSet(_inputAspectsName);
		for (auto node : _inputs) {
			auto & inputAspect = inputAspects.findApplicableAspect(node->name());
			hashes.push_back(node->hashOf(inputAspect.name()));
		}
		// TODO: add hash of inputAspects because changes in input aspects definition
		// must result in re-execution of the command.
		XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
		return hash;	
	}

	void CommandNode::startSelf() {
		auto d = Delegate<void>::CreateLambda([this]() { execute(); });
		_context->threadPoolQueue().push(std::move(d));
	}

	void CommandNode::getSourceInputs(std::vector<std::shared_ptr<Node>> & sourceInputs) const {
		for (auto i : _inputs) {
			auto file = dynamic_pointer_cast<SourceFileNode>(i);
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
			if (ofile->state() != State::Ok) {
				throw std::runtime_error("state error");
			}
			ofile->rehashAll();
		}
	}

	void CommandNode::setInputs(std::vector<std::shared_ptr<FileNode>> const& newInputs) {
		if (_inputs != newInputs) {
			for (auto i : _inputs) i->removePreParent(this);
			_inputs = newInputs;
			for (auto i : _inputs) i->addPreParent(this);
		}
	}

	// TODO: detect in/output files, handle cancelation, log script output
	//
	Node::State CommandNode::executeScript() {
		std::filesystem::path tmpDir = FileSystem::createUniqueDirectory();

		std::filesystem::path scriptFilePath(tmpDir / "cmdScript.cmd");
	    std::ofstream scriptFile(scriptFilePath.string());
		scriptFile << _script << std::endl;
		scriptFile.close();
		scriptFilePath = FileSystem::normalizePath(scriptFilePath);

		std::map<std::string, std::string> env;
		env["TMP"] = tmpDir.string();

		_scriptExecutor = std::make_unique<MonitoredProcess>(
			cmdExe, 
			std::string("/c") + scriptFilePath.string(),
			env);
		MonitoredProcessResult result = _scriptExecutor->wait();
		_scriptExecutor = nullptr;

		if (result.exitCode == 0) {
			std::cout << "Succesfully executed cmd " << name().string() << std::endl;
			std::filesystem::remove_all(tmpDir);
		} else {
			std::cout << "Failed to execute cmd: " << name().string() << std::endl;
			std::cout << "Temporary result directory: " << tmpDir.string() << std::endl;
			if (!result.stdOut.empty()) {
				std::cout << "stdout: " << std::endl << result.stdOut << std::endl;
			}
			if (!result.stdErr.empty()) {
				std::cout << "stderr: " << std::endl << result.stdErr << std::endl;
			}
		}
		char buf[32];
		unsigned int illegalInputs = 0;
		std::vector<std::shared_ptr<FileNode>> newInputs;
		setInputs(newInputs);
		for (auto const& path : result.inputOnlyFiles) {
			auto node = context()->nodes().find(path);
			auto fileNode = dynamic_pointer_cast<FileNode>(node);
			if (fileNode == nullptr) {
				if (path != scriptFilePath) {
					illegalInputs++;
					std::cout << "Not an allowed input: " << path.string() << std::endl;
				}
			} else {
				newInputs.push_back(fileNode);
			}
		}
		setInputs(newInputs);
		if (illegalInputs != 0) {
			std::cout << name() << " has " << itoa(illegalInputs, buf, 10) << " not-allowed input files" << std::endl;
		}
		unsigned int illegalOutputs = 0;
		for (auto const& path : result.outputFiles) {
			auto node = context()->nodes().find(path);
			auto generatedNode = dynamic_pointer_cast<GeneratedFileNode>(node);
			if (generatedNode == nullptr || generatedNode->producer().get() != this) {
				illegalOutputs++;
				std::cout << "Not an allowed output: " << path.string() << std::endl;
			} else {
				generatedNode->rehashAll();
			}
		}
		if (illegalOutputs != 0) {
			std::cout << name() << " has " << itoa(illegalOutputs, buf, 10) << " not-allowed output files" << std::endl;
		}

		Node::State newState = (result.exitCode == 0) ? State::Ok : State::Failed;
		return newState;
	}

	void CommandNode::cancelSelf() {
		auto executor = _scriptExecutor;
		if (executor != nullptr) executor->terminate();
	}

	void CommandNode::execute() {

		State newState = executeScript();
		// todo: process detected input files as described in FileNode.h
		// scenarios 1 and 3. Make sure to set this as parent of source
		// input files and to assert that generated input files are
		// outputs of the _inputProducers.
		// TODO: the order of the input nodes is important for the hash 
		// computation. Therefore sort the inputs by name.

		rehashOutputs();
		_executionHash = computeExecutionHash();

		postSelfCompletion(newState);
	}
}
