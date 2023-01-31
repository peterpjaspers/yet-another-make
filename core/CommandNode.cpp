#include "CommandNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "ExecutionContext.h"
#include "MonitoredProcess.h"
#include "FileAspectSet.h"
#include "FileSystem.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <utility>
#include <boost/process.hpp>

namespace
{
	using namespace YAM;

	std::string cmdExe = boost::process::search_path("cmd").string();

	template <typename V>
	bool contains(std::vector<V> const& vec, V el) {
		return std::find(vec.begin(), vec.end(), el) != vec.end();
	}
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

	std::shared_ptr<FileNode> CommandNode::findInputNode(
		std::filesystem::path const& input,
		std::filesystem::path const& exclude
	) {
		bool valid = true;
		auto node = context()->nodes().find(input);
		auto fileNode = dynamic_pointer_cast<FileNode>(node);
		if (fileNode == nullptr) {
			if (input != exclude) {
				valid = false;
				std::cout
					<< "Not an allowed input: " << input.string() << std::endl
					<< "Reason: it is not contained in a known file repository."
					<< std::endl;
			}
		} else {
			auto genFileNode = dynamic_pointer_cast<GeneratedFileNode>(fileNode);
			auto srcFileNode = dynamic_pointer_cast<SourceFileNode>(fileNode);
			if (genFileNode != nullptr) {
				if (!contains(_inputProducers, genFileNode->producer())) {
					valid = false;
					std::cout
						<< "Not an allowed input: " << input.string() << std::endl
						<< "Reason: file may not yet be up-to-date because its producer(" << genFileNode->producer()->name().string()
						<< ") is not a prerequisite of producer(" << name().string() << ")"
						<< " and therefore proper build order is not guaranteed."
						<< std::endl;
				}
			} else if (srcFileNode == nullptr) {
				valid = false;
				std::cout
					<< "Not an allowed input: " << input.string() << std::endl
					<< "Reason: path not associated with a file node."
					<< std::endl;
			}
		}
		return valid ? fileNode : nullptr;
	}

	bool CommandNode::findInputNodes(
		MonitoredProcessResult const& result,
		std::filesystem::path const& exclude,
		std::vector<std::shared_ptr<FileNode>>& inputNodes
	) {
		unsigned int illegals = 0;
		for (auto const& path : result.readOnlyFiles) {
			std::shared_ptr<FileNode> fileNode = findInputNode(path, exclude);
			if (fileNode == nullptr) {
				if (path != exclude) illegals++;
			} else {
				inputNodes.push_back(fileNode);
			}
		}
		if (illegals != 0) {
			char buf[32];
			std::cout << name().string() << " has " << itoa(illegals, buf, 10) << " not-allowed input files" << std::endl;
		}
		return illegals == 0;
	}

	std::shared_ptr<GeneratedFileNode> CommandNode::findOutputNode(
		std::filesystem::path const& output
	) {
		bool valid = true;
		std::shared_ptr<Node> node = context()->nodes().find(output);
		auto genFileNode = dynamic_pointer_cast<GeneratedFileNode>(node);
		if (genFileNode == nullptr) {
			auto srcFileNode = dynamic_pointer_cast<SourceFileNode>(node);
			if (srcFileNode != nullptr) {
				valid = false;
				std::cout
					<< "Not an allowed output: " << output.string() << std::endl
					<< "Reason: source files must be accessed read-only."
					<< std::endl;
			} else {
				valid = false;
				std::cout
					<< "Not an allowed output: " << output.string() << std::endl
					<< "Reason: not declared as output of " << name().string()
					<< std::endl;
			}
		} else if (genFileNode->producer().get() != this) {
			valid = false;
			std::cout
				<< "Not an allowed output: " << output.string() << std::endl
				<< "Reason: output producer(" << genFileNode->producer()->name().string()
				<< ") is not the expected producer(" << name().string() << ")."
				<< std::endl;
		}
		return valid ? genFileNode : nullptr;
	}

	bool CommandNode::findOutputNodes(
		MonitoredProcessResult const& result,
		std::vector<std::shared_ptr<GeneratedFileNode>>& outputNodes
	) {
		unsigned int illegals = 0;
		for (auto const& path : result.writtenFiles) {
			std::shared_ptr<GeneratedFileNode> fileNode = findOutputNode(path);
			if (fileNode == nullptr) {
				illegals++;
			} else {
				outputNodes.push_back(fileNode);
			}
		}
		if (illegals != 0) {
			char buf[32];
			std::cout << name().string() << " has " << itoa(illegals, buf, 10) << " not-allowed input files" << std::endl;
		}
		return illegals == 0;
	}

	bool CommandNode::verifyOutputNodes(
		std::vector<std::shared_ptr<GeneratedFileNode>>& newOutputs
	) {
		bool valid = true;
		std::vector<std::shared_ptr<GeneratedFileNode>> inBoth;
		std::vector<std::shared_ptr<GeneratedFileNode>> inThisOnly;
		std::vector<std::shared_ptr<GeneratedFileNode>> inNewOnly;
		std::set_intersection(
			_outputs.begin(), _outputs.end(),
			newOutputs.begin(), newOutputs.end(),
			std::inserter(inBoth, inBoth.begin()));
		std::set_difference(
			_outputs.begin(), _outputs.end(),
			inBoth.begin(), inBoth.end(),
			std::inserter(inThisOnly, inThisOnly.begin()));
		std::set_difference(
			newOutputs.begin(), newOutputs.end(),
			inBoth.begin(), inBoth.end(),
			std::inserter(inNewOnly, inNewOnly.begin()));
		if (!inThisOnly.empty() || !inNewOnly.empty()) {
			valid = false;
			std::cout << "Unexpected outputs for " << name().string() << std::endl;
			std::cout << "Expected outputs: ";
			for (auto const& n : _outputs) std::cout << "    " << n->name().string() << std::endl;
		    std::cout << "Actual outputs: ";
			for (auto const& n : newOutputs) std::cout << "    " << n->name().string() << std::endl;
		}
		return valid;
	}

	void CommandNode::cancelSelf() {
		auto executor = _scriptExecutor;
		if (executor != nullptr) executor->terminate();
	}

	MonitoredProcessResult CommandNode::executeScript(std::filesystem::path& scriptFilePath) {
		std::filesystem::path tmpDir = FileSystem::createUniqueDirectory();
	    scriptFilePath = std::filesystem::path(tmpDir / "cmdscript.cmd");
		std::ofstream scriptFile(scriptFilePath.string());
		scriptFile << _script << std::endl;
		scriptFile.close();

		std::map<std::string, std::string> env;
		env["TMP"] = tmpDir.string();

		_scriptExecutor = std::make_unique<MonitoredProcess>(
			cmdExe,
			std::string("/c ") + scriptFilePath.string(),
			env);
		MonitoredProcessResult result = _scriptExecutor->wait();
		_scriptExecutor = nullptr;

		if (result.exitCode != 0) {
			std::cout << "Failed to execute cmd: " << name().string() << std::endl;
			std::cout << "Temporary result directory: " << tmpDir.string() << std::endl;
			if (!result.stdOut.empty()) {
				std::cout << "stdout: " << std::endl << result.stdOut << std::endl;
			}
			if (!result.stdErr.empty()) {
				std::cout << "stderr: " << std::endl << result.stdErr << std::endl;
			}
		} else {
			std::cout << "Succesfully executed cmd " << name().string() << std::endl;
			std::filesystem::remove_all(tmpDir);
		}
		return std::move(result);
	}

	void CommandNode::execute() {
		Node::State newState;
		std::filesystem::path scriptFilePath;
		MonitoredProcessResult result = executeScript(scriptFilePath);
		if (result.exitCode != 0) {
			newState = Node::State::Failed;
			_executionHash = rand();
		} else {
			std::vector<std::shared_ptr<FileNode>> newInputs;
			bool validInputs = findInputNodes(result, scriptFilePath, newInputs);

			std::vector<std::shared_ptr<GeneratedFileNode>> newOutputs;
			bool validOutputs = findOutputNodes(result, newOutputs);
			validOutputs = validOutputs && verifyOutputNodes(newOutputs);

			newState = (validInputs && validOutputs) ? Node::State::Ok : Node::State::Failed;
			if (newState == Node::State::Ok) {
				setInputs(newInputs);
				rehashOutputs();
				_executionHash = computeExecutionHash();
			}
		}
		postSelfCompletion(newState);
	}
}
