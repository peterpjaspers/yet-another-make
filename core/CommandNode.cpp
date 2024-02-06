#include "CommandNode.h"
#include "DirectoryNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "GroupNode.h"
#include "ExecutionContext.h"
#include "MonitoredProcess.h"
#include "FileAspectSet.h"
#include "FileSystem.h"
#include "FileRepository.h"
#include "BuildFileCompiler.h"
#include "NodeMapStreamer.h"
#include "IStreamer.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <utility>
#include <unordered_set>
#include <boost/process.hpp>

namespace
{
    using namespace YAM;

    std::string cmdExe = boost::process::search_path("cmd").string();
    uint32_t streamableTypeId = 0;

    template <typename V>
    bool contains(std::vector<V> const& vec, V el) {
        return std::find(vec.begin(), vec.end(), el) != vec.end();
    }

    std::vector<std::shared_ptr<Node>> expandGroups(std::vector<std::shared_ptr<Node>> const& nodes) {
        std::vector<std::shared_ptr<Node>> expanded;
        for (auto const& node : nodes) {
            auto groupNode = dynamic_pointer_cast<GroupNode>(node);
            if (groupNode != nullptr) {
                auto const& content = groupNode->group();
                expanded.insert(expanded.end(), content.begin(), content.end());
            } else {
                expanded.push_back(node);
            }
        }
        return expanded;
    }

    void getGroups(
        std::vector<std::shared_ptr<Node>> const& nodes,
        std::vector<std::shared_ptr<GroupNode>>& groups
    ) {
        for (auto const& node : nodes) {
            auto grp = dynamic_pointer_cast<GroupNode>(node);
            if (grp != nullptr) groups.push_back(grp);
        }
    }

    void computePathSetsDifference(
        std::set<std::filesystem::path> const& in1,
        std::set<std::filesystem::path> const& in2,
        std::set<std::filesystem::path>& inBoth,
        std::set<std::filesystem::path>& onlyIn1,
        std::set<std::filesystem::path>& onlyIn2
    ) {
        std::set_intersection(
            in1.begin(), in1.end(),
            in2.begin(), in2.end(),
            std::inserter(inBoth, inBoth.begin()));
        std::set_difference(
            in1.begin(), in1.end(),
            inBoth.begin(), inBoth.end(),
            std::inserter(onlyIn1, onlyIn1.begin()));
        std::set_difference(
            in2.begin(), in2.end(),
            inBoth.begin(), inBoth.end(),
            std::inserter(onlyIn2, onlyIn2.begin()));
    }

    bool isGenerated(std::shared_ptr<Node> const& node) {
        return nullptr != dynamic_cast<GeneratedFileNode*>(node.get());
    }

    void _addObserver(StateObserver* command, std::shared_ptr<Node> const& node) {
        auto genFileNode = dynamic_cast<GeneratedFileNode*>(node.get());
        if (genFileNode) {
            genFileNode->producer()->addObserver(command);
        } else {
            node->addObserver(command);
        }
    }

    void _removeObserver(StateObserver* command, std::shared_ptr<Node> const& node) {
        auto genFileNode = dynamic_cast<GeneratedFileNode*>(node.get());
        if (genFileNode) {
            genFileNode->producer()->removeObserver(command);
        } else {
            node->removeObserver(command);
        }
    }

    void getOutputFileNodes(
        std::unordered_set<Node*> const& producers,
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>>& outputFiles
    ) {
        std::vector<std::shared_ptr<Node>> outputs;
        for (auto const& producer : producers) producer->getOutputs(outputs);

        for (auto const& output : outputs) {
            auto genFileNode = dynamic_pointer_cast<GeneratedFileNode>(output);
            if (genFileNode != nullptr) {
                outputFiles.insert(std::pair{ genFileNode->name(), genFileNode });
            }
        }
    }

    void logBuildOrderNotGuaranteed(
        CommandNode* cmd,
        GeneratedFileNode* inputFile,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Build order is not guaranteed." << std::endl
            << "Fix: declare input file as input of command." << std::endl
            << "Command   : " << cmd->name().string() << std::endl
            << "Input file: " << inputFile->absolutePath().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logInputNotInARepository(
        CommandNode* cmd,
        std::filesystem::path const& inputFile,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Input file ignored because not in a known file repository." << std::endl
            << "Fix: declare the file repository that contains the input," << std::endl
            << "or change command script to not depend on the input file." << std::endl
            << "Command   : " << cmd->name().string() << std::endl
            << "Input file: " << inputFile.string() << std::endl;
        LogRecord record(LogRecord::IgnoredInputFiles, ss.str());
        logBook.add(record);
    }

    void logWriteAccessedSourceFile(
        CommandNode* cmd,
        SourceFileNode* outputFile,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Source file is updated by build." << std::endl
            << "Fix: change command script to not update the source file." << std::endl
            << "Command    : " << cmd->name().string() << std::endl
            << "Source file: " << outputFile->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }
    
    void logOutputFileNotDeclared(
        CommandNode* cmd,
        std::filesystem::path const& outputFile,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Unknown output file."
            << "Fix: declare the file as output of command." << std::endl
            << "Command    : " << cmd->name().string() << std::endl
            << "Output file: " << outputFile.string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logAlreadyProducedByOtherCommand(
        CommandNode* cmd,
        GeneratedFileNode* outputFile,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Output file is produced by 2 commands." << std::endl
            << "Fix: adapt command script to ensure that file is produced by one command only." << std::endl
            << "Command 1  : " << outputFile->producer()->name().string() << std::endl
            << "Command 2  : " << cmd->name().string() << std::endl
            << "Output file: " << outputFile->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logUnexpectedOutputs(
        CommandNode* cmd,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& declared,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& actual,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Mismatch between declared outputs and actual outputs." << std::endl
            << "Fix: declare outputs and/or modify scripts and/or modify output file names." << std::endl
            << "Command : " << cmd->name().string() << std::endl
            << "Declared outputs: " << std::endl;
        for (auto const& n : declared) ss << "    " << n->name().string() << std::endl;
         ss << "Actual outputs  : " << std::endl;
        for (auto const& n : actual) ss << "    " << n->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logScriptFailure(
        CommandNode* cmd,
        MonitoredProcessResult const& result,
        std::filesystem::path tmpDir,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Command script failed." << std::endl
            << "Command: " << cmd->name().string() << std::endl
            << "Temporary result directory: " << tmpDir.string() << std::endl;
        if (!result.stdOut.empty()) {
            ss << "script stdout: " << std::endl << result.stdOut << std::endl;
        }
        if (!result.stdErr.empty()) {
            ss << "script stderr: " << std::endl << result.stdErr << std::endl;
        }
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logDirRemovalError(
        CommandNode* cmd,
        std::filesystem::path const& dir, 
        std::error_code ec,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Failed to delete temporary script directory." << std::endl
            << "Command : " << cmd->name().string() << std::endl
            << "Tmp dir : " << dir.string() << std::endl
            << "Reason: " << ec.message() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }
}

namespace YAM
{
    using namespace boost::process;

    CommandNode::CommandNode()
        : Node()
        , _buildFile(nullptr) {}

    CommandNode::CommandNode(
        ExecutionContext* context,
        std::filesystem::path const& name)
        : Node(context, name)
        , _buildFile(nullptr)
        , _inputAspectsName(FileAspectSet::entireFileSet().name())
        , _executionHash(rand())
    {}

    CommandNode::~CommandNode() {
        for (auto output : _outputs) {
            output->removeObserver(this);
        }
        for (auto producer : _inputProducers) {
            producer->removeObserver(this);
        }
        clearDetectedInputs();
    }

    void CommandNode::inputAspectsName(std::string const& newName) {
        if (_inputAspectsName != newName) {
            _inputAspectsName = newName;
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::cmdInputs(std::vector<std::shared_ptr<Node>> const& newInputs) {
        if (_cmdInputs != newInputs) {
            _cmdInputs = newInputs;
            updateInputProducers();
            _executionHash = rand();
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::orderOnlyInputs(std::vector<std::shared_ptr<Node>> const& newInputs) {
        if (_orderOnlyInputs != newInputs) {
            _orderOnlyInputs = newInputs;
            updateInputProducers();
            _executionHash = rand();
            modified(true);
            setState(State::Dirty);
        }
    }

    void appendInputProducers(
        std::vector<std::shared_ptr<Node>>const& inputs,
        std::unordered_set<Node*>& inputProducers
    ) {
        for (auto const& node : inputs) {
            auto genFileNode = dynamic_pointer_cast<GeneratedFileNode>(node);
            if (genFileNode != nullptr) {
                inputProducers.insert(genFileNode->producer());
            } else {
                auto groupNode = dynamic_pointer_cast<GroupNode>(node);
                if (groupNode != nullptr) inputProducers.insert(groupNode.get());
            }
        }
    }

    void CommandNode::updateInputProducers() {
        for (auto producer : _inputProducers) {
            producer->removeObserver(this);
        }
        _inputProducers.clear();
        appendInputProducers(_cmdInputs, _inputProducers);
        appendInputProducers(_orderOnlyInputs, _inputProducers);
        for (auto producer : _inputProducers) {
            producer->addObserver(this);
        }
    }

    void CommandNode::outputs(std::vector<std::shared_ptr<GeneratedFileNode>> const & newOutputs) {
        if (_outputs != newOutputs) {
            for (auto output : _outputs) {
                output->deleteFile(true);
                output->removeObserver(this);
            }
            _outputs = newOutputs;
            for (auto output : _outputs) output->addObserver(this);
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::ignoreOutputs(std::vector<std::filesystem::path> const& newOutputs) {
        if (_ignoredOutputs != newOutputs) {
            _ignoredOutputs = newOutputs;
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::script(std::string const& newScript) {
        if (newScript != _script) {
            _script = newScript;
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::workingDirectory(std::shared_ptr<DirectoryNode> const& dir) {
        if (_workingDir.lock() != dir) {
            _workingDir = dir;
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        for (auto const& n : _outputs) outputs.push_back(dynamic_pointer_cast<Node>(n));
    }

    void CommandNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
        for (auto const& pair : _detectedInputs) inputs.push_back(dynamic_pointer_cast<Node>(pair.second));
    }

    std::vector<std::shared_ptr<GroupNode>> CommandNode::inputGroups() const {
        std::vector<std::shared_ptr<GroupNode>> groups;
        getGroups(_cmdInputs, groups);
        getGroups(_orderOnlyInputs, groups);
        return groups;
    }

    void CommandNode::buildFile(SourceFileNode* buildFile) {
        _buildFile = buildFile;
    }

    void CommandNode::ruleLineNr(std::size_t ruleLineNr) {
        _ruleLineNr = ruleLineNr;
    }
    SourceFileNode const* CommandNode::buildFile() const {
        return _buildFile;
    }
    std::size_t CommandNode::ruleLineNr() const {
        return _ruleLineNr;
    }

    XXH64_hash_t CommandNode::computeExecutionHash() const {
        std::vector<XXH64_hash_t> hashes;
        auto wdir = _workingDir.lock();
        if (wdir != nullptr) hashes.push_back(XXH64_string(wdir->name().string()));
        hashes.push_back(XXH64_string(_script));
        for (auto const& node : _cmdInputs) {
            auto grpNode = dynamic_pointer_cast<GroupNode>(node);
            if (grpNode != nullptr) hashes.push_back(grpNode->hash());
        }
        for (auto const& node : _orderOnlyInputs) {
            auto grpNode = dynamic_pointer_cast<GroupNode>(node);
            if (grpNode != nullptr) hashes.push_back(grpNode->hash());
        }
        for (auto const& path : _ignoredOutputs) {
            hashes.push_back(XXH64_string(path.string()));
        }
        auto entireFile = FileAspect::entireFileAspect().name();
        for (auto const& node : _outputs) hashes.push_back(node->hashOf(entireFile));
        FileAspectSet inputAspects = context()->findFileAspectSet(_inputAspectsName);
        for (auto const& pair : _detectedInputs) {
            auto const& node = pair.second;
            auto& inputAspect = inputAspects.findApplicableAspect(node->name());
            hashes.push_back(node->hashOf(inputAspect.name()));
        }
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    void CommandNode::getSourceInputs(std::vector<Node*>& sourceInputs) const {
        for (auto const& pair : _detectedInputs) {
            auto const& node = pair.second;
            auto file = dynamic_pointer_cast<SourceFileNode>(node);
            if (file != nullptr) sourceInputs.push_back(file.get());
        }
    }

    void CommandNode::clearDetectedInputs() {
        for (auto const& pair : _detectedInputs) {
            auto const& input = pair.second;
            if (!isGenerated(input)) input->removeObserver(this);
        }
        _detectedInputs.clear();        
    }

    void CommandNode::setDetectedInputs(ExecutionResult const& result) {
        // Note that the producer of an input GeneratedFileNode is a requisite
        // of the command node, not the GeneratedFileNode itself. 
        // A command node therefore does not register itself as observer of an
        // input GeneratedFileNode. Instead it registers itself as observer of
        // the producer of the input GeneratedFileNode (in function 
        // observeInputProducers(). This prevents a spurious callback to 
        // Node::handleCompletionOf(Node* genFileNode) from the input
        // GeneratedFileNode to the command node.
        // Note: Dirty propagation in case of tampering with generated files
        // remains intact because a GeneratedFileNode propagates Dirty to its 
        // producing CommandNode who then notifies its observers, i.e to nodes
        // that read one or more output files of the producing command node.
        // 
        for (auto const& symPath : result._removedInputPaths) {  
            auto it = _detectedInputs.find(symPath);
            if (it == _detectedInputs.end()) throw std::exception("no such input");
            auto const& node = it->second;
            if (!isGenerated(node)) node->removeObserver(this);
            _detectedInputs.erase(it);
        }
        for (auto const& node : result._addedInputNodes) {
            if (!isGenerated(node)) node->addObserver(this);
            auto result = _detectedInputs.insert({ node->name(), node });
            if (!result.second) throw std::exception("attempt to add duplicate input");
        }
        modified(!result._removedInputPaths.empty() || !result._addedInputNodes.empty());
    }

    std::shared_ptr<GeneratedFileNode> CommandNode::findOutputNode(
        std::filesystem::path const& outputSymPath,
        MemoryLogBook& logBook
    ) {
        bool valid = true;
        std::shared_ptr<GeneratedFileNode> outputNode;
        std::shared_ptr<Node> node = context()->nodes().find(outputSymPath);
        outputNode = dynamic_pointer_cast<GeneratedFileNode>(node);
        if (outputNode == nullptr) {
            auto srcFileNode = dynamic_pointer_cast<SourceFileNode>(node);
            if (srcFileNode != nullptr) {
                logWriteAccessedSourceFile(this, srcFileNode.get(), logBook);
            } else {
                valid = false;
                logOutputFileNotDeclared(this, outputSymPath, logBook);
            }
        } else if (outputNode->producer() != this) {
            valid = false;
            logAlreadyProducedByOtherCommand(this, outputNode.get(), logBook);
        }
        return valid ? outputNode : nullptr;
    }

    bool CommandNode::findOutputNodes(
        std::set<std::filesystem::path> const& outputSymPaths,
        std::vector<std::shared_ptr<GeneratedFileNode>>& outputNodes,
        MemoryLogBook& logBook
    ) {
        unsigned int illegals = 0;
        for (auto const& symPath : outputSymPaths) {
            std::shared_ptr<GeneratedFileNode> fileNode = findOutputNode(symPath, logBook);
            if (fileNode == nullptr) {
                illegals++;
            } else {
                outputNodes.push_back(fileNode);
            }
        }
        return illegals == 0;
    }

    bool CommandNode::verifyOutputNodes(
        std::vector<std::shared_ptr<GeneratedFileNode>> const& newOutputs,
        MemoryLogBook& logBook
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
            logUnexpectedOutputs(this, _outputs, newOutputs, logBook);
        }
        return valid;
    }

    void CommandNode::cancel() {
        Node::cancel();
        if (canceling()) {
            std::shared_ptr<IMonitoredProcess> executor = _scriptExecutor.load();
            if (executor != nullptr) {
                executor->terminate(); 
                _scriptExecutor.store(nullptr);
            }
        }
    }

    // main thread
    void CommandNode::start() {
        Node::start();
        std::vector<Node*> requisites;
        for (auto const& ip : _inputProducers) requisites.push_back(ip);
        getSourceInputs(requisites);
        for (auto const& n : _outputs) requisites.push_back(n.get());
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State state) { handleRequisitesCompletion(state); }
        );
        startNodes(requisites, std::move(callback));
    }

    void CommandNode::handleRequisitesCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            Node::notifyCompletion(state);
        } else if (canceling()) {
            Node::notifyCompletion(Node::State::Canceled);
        } else if (_executionHash != computeExecutionHash()) {
            context()->statistics().registerSelfExecuted(this);
            auto d = Delegate<void>::CreateLambda(
                [this]() { executeScript(); }
            );
            context()->threadPoolQueue().push(std::move(d));
        } else {
            Node::notifyCompletion(state);
        }
    }

    std::filesystem::path CommandNode::convertToSymbolicPath(
        std::filesystem::path const& absPath, MemoryLogBook& logBook
    ) {
        auto repo = context()->findRepositoryContaining(absPath);
        if (repo == nullptr) {
            std::stringstream ss;
            ss << "File " << absPath.string() << "is used as input or output by script " << std::endl;
            ss << _script << std::endl;
            if (_buildFile != nullptr) {
                ss << "This script is defined in buildfile " << _buildFile->name() << " by the rule at line " << _ruleLineNr << std::endl;
            }
            ss << "This file is not part of a known file repository and will not be tracked for changes." << std::endl;
            ss << "To get rid of this warning you must declare the file repository that contains the file." << std::endl;
            ss << "Set the repository tracked property to false if yam must not track files in this repository." << std::endl;
            LogRecord warning(LogRecord::Warning, ss.str());
            logBook.add(warning);
            return "";
        } else {
            return repo->symbolicPathOf(absPath);
        }
    }

    std::set<std::filesystem::path> CommandNode::convertToSymbolicPaths(
        std::set<std::filesystem::path> const& absPaths, MemoryLogBook& logBook
    ) {
        std::set<std::filesystem::path> symPaths;
        for (auto const& absPath : absPaths) {
            auto symPath = convertToSymbolicPath(absPath, logBook);
            if (!symPath.empty()) symPaths.insert(symPath);
        }
        return symPaths;
    }

    // threadpool
    void CommandNode::executeScript() {
        auto result = std::make_shared<ExecutionResult>();
        result->_newState = Node::State::Ok;
        if (canceling()) {
            result->_newState = Node::State::Canceled;
        } else {
            MonitoredProcessResult scriptResult = executeMonitoredScript(result->_log);
            if (scriptResult.exitCode != 0) {
                result->_newState = canceling() ? Node::State::Canceled : Node::State::Failed;
            } else {
                result->_newState = Node::State::Ok;
                std::set<std::filesystem::path> previousInputPaths;
                for (auto const& pair : _detectedInputs) {
                    previousInputPaths.insert(pair.second->name());
                }                    
                auto currentInputPaths = convertToSymbolicPaths(scriptResult.readOnlyFiles, result->_log);
                computePathSetsDifference(
                    previousInputPaths, currentInputPaths,
                    result->_keptInputPaths,
                    result->_removedInputPaths,
                    result->_addedInputPaths);
                result->_outputPaths = convertToSymbolicPaths(scriptResult.writtenFiles, result->_log);
                if (_postProcessor != nullptr) {
                    _postProcessor->process(scriptResult);
                }
            }
        }
        auto d = Delegate<void>::CreateLambda(
            [this, result]() { handleExecuteScriptCompletion(result); }
        );
        context()->mainThreadQueue().push(std::move(d));
    }

    void CommandNode::handleExecuteScriptCompletion(std::shared_ptr<ExecutionResult> sresult) {
        ExecutionResult& result = *sresult;
        if (result._newState != Node::State::Ok) {
        } else if (canceling()) {
            result._newState = Node::State::Canceled;
            result._log.clear();
        } else {
            std::vector<std::shared_ptr<GeneratedFileNode>> outputNodes;
            bool validOutputs = findOutputNodes(result._outputPaths, outputNodes, result._log);
            validOutputs = validOutputs && verifyOutputNodes(outputNodes, result._log);
            if (!validOutputs) {
                result._newState = Node::State::Failed;
            } else {
                std::vector< std::shared_ptr<Node>> outputsAndNewInputs;
                // output nodes have been updated by command script, hence their
                // hashes need to be re-computed.
                for (auto& n : _outputs) {
                    // temporarily stop observing n to avoid Dirty state to
                    // propagate to this command (see handleDirtyOf()).
                    n->removeObserver(this);
                    n->setState(Node::State::Dirty);
                    n->addObserver(this);
                    outputsAndNewInputs.push_back(n);
                }
                std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> allowedGenInputFiles;
                getOutputFileNodes(_inputProducers, allowedGenInputFiles);
                std::vector<std::shared_ptr<FileNode>> notUsed1;
                std::vector<std::shared_ptr<Node>> notUsed2;
                // Kept inputs must be validated because of possible change in 
                // _inputproducers.
                bool validKeptInputs = findInputNodes(
                    allowedGenInputFiles,
                    result._keptInputPaths,
                    notUsed1,
                    notUsed2,
                    result._log);
                bool validNewInputs = findInputNodes(
                    allowedGenInputFiles,
                    result._addedInputPaths,
                    result._addedInputNodes,
                    outputsAndNewInputs,
                    result._log);
                if (validKeptInputs && validNewInputs) {
                    setDetectedInputs(result);
                    std::vector<Node*> rawNodes;
                    for (auto const& n : outputsAndNewInputs) rawNodes.push_back(n.get());
                    auto callback = Delegate<void, Node::State>::CreateLambda(
                        [this, sresult](Node::State state) { 
                            handleOutputAndNewInputFilesCompletion(state, sresult); }
                    );
                    startNodes(rawNodes, std::move(callback));
                } else {
                    result._newState = Node::State::Failed;
                }
            }
        }
        result._log.forwardTo(*(context()->logBook()));
        if (result._newState != Node::State::Ok) {
            handleOutputAndNewInputFilesCompletion(result._newState, sresult);
        }
    }

    void CommandNode::handleOutputAndNewInputFilesCompletion(Node::State newState, std::shared_ptr<ExecutionResult> sresult) {
        ExecutionResult& result = *sresult;
        result._newState = newState;
        if (newState == Node::State::Ok) {
            auto prevHash = _executionHash;
            _executionHash = computeExecutionHash();
            if (
                _postProcessor == nullptr 
                && prevHash != _executionHash 
                && context()->logBook()->mustLogAspect(LogRecord::Aspect::DirectoryChanges)
            ) {
                std::stringstream ss;
                ss << className() << " " << name().string() << " has changed inputs/outputs/script/file deps";
                LogRecord change(LogRecord::DirectoryChanges, ss.str());
                context()->logBook()->add(change);
            }
        } else {
            ExecutionResult r;
            for (auto const& pair : _detectedInputs) r._removedInputPaths.insert(pair.first);
            setDetectedInputs(r);
            for (auto output : _outputs) output->deleteFile();
            _executionHash = rand();
        }
        modified(true);
        notifyCommandCompletion(sresult);
    }

    void CommandNode::notifyCommandCompletion(std::shared_ptr<ExecutionResult> sresult) {
        Node::notifyCompletion(sresult->_newState);
    }

    bool CommandNode::verifyCmdFlag(
        std::string const& script, 
        std::vector<std::shared_ptr<Node>> const& cmdInputs,
        ILogBook& logBook
    ) {
        bool containsFlag = BuildFileCompiler::containsCmdInputFlag(script); 
        bool valid = !(containsFlag && cmdInputs.empty());
        if (!valid) {
            std::string buildFile = _buildFile != nullptr ? _buildFile->name().string() : "";
            std::stringstream ss;
            ss << "In command of rule at line " << _ruleLineNr << " in buildfile " << buildFile << ":" << std::endl;
            ss << "No cmd input files while command contains percentage flag that operates on cmd input." << std::endl;
            LogRecord error(LogRecord::Error, ss.str());
            logBook.add(error);
        }
        return valid;
    }
    bool CommandNode::verifyOrderOnlyFlag(
        std::string const& script,
        std::vector<std::shared_ptr<Node>> const& orderOnlyInputs,
        ILogBook& logBook
    ) {
        bool containsFlag = BuildFileCompiler::containsOrderOnlyInputFlag(script);
        bool valid = !(containsFlag && orderOnlyInputs.empty());
        if (!valid) {
            std::string buildFile = _buildFile != nullptr ? _buildFile->name().string() : "";
            std::stringstream ss;
            ss << "In command of rule at line " << _ruleLineNr << " in buildfile " << buildFile << ":" << std::endl;
            ss << "No order-only input files while command contains percentage flag that operates on order-only input." << std::endl;
            LogRecord error(LogRecord::Error, ss.str());
            logBook.add(error);
        }
        return valid;
    }

    bool CommandNode::verifyOutputFlag(
        std::string const& script,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs,
        ILogBook& logBook
    ) {
        bool valid = !outputs.empty();
        if (!valid) {
            valid = BuildFileCompiler::containsOutputFlag(script);
            if (!valid) {
                std::string buildFile = _buildFile != nullptr ? _buildFile->name().string() : "";
                std::stringstream ss;
                ss << "In command of rule at line " << _ruleLineNr << " in buildfile " << buildFile << ":" << std::endl;
                ss << "No output files while command contains percentage flag that operates on cmd output." << std::endl;
                LogRecord error(LogRecord::Error, ss.str());
                logBook.add(error);
            }
        }
        return valid;
    }

    std::string CommandNode::compileScript(ILogBook& logBook) {
        std::filesystem::path wdir;
        auto workingDir = _workingDir.lock();
        if (workingDir == nullptr) return _script;
        if (BuildFileCompiler::isLiteralScript(_script)) return _script;

        BuildFile::Script bfScript;
        bfScript.script = _script;
        std::filesystem::path buildFile = _buildFile == nullptr ? "" : _buildFile->name();
        std::vector<std::shared_ptr<Node>> cmdInputs = expandGroups(_cmdInputs);
        std::vector<std::shared_ptr<Node>> orderOnlyInputs = expandGroups(_cmdInputs);

        bool cmdInOk = verifyCmdFlag(_script, cmdInputs, logBook);
        bool orderOnlyInOk = verifyOrderOnlyFlag(_script, orderOnlyInputs, logBook);
        bool outputOk = verifyOutputFlag(_script, _outputs, logBook);
        if (cmdInOk && orderOnlyInOk && outputOk) {
            std::string script = BuildFileCompiler::compileScript(
                buildFile,
                bfScript,
                workingDir.get(),
                cmdInputs,
                orderOnlyInputs,
                _outputs);
            return script;
        }
        return "";
    }

    // threadpool
    MonitoredProcessResult CommandNode::executeMonitoredScript(ILogBook& logBook) {
        for (auto output : _outputs) output->deleteFile();

        std::string script = compileScript(logBook);
        if (script.empty()) return MonitoredProcessResult{ 0 };

        std::filesystem::path tmpDir = FileSystem::createUniqueDirectory();
        auto scriptFilePath = std::filesystem::path(tmpDir / "cmdscript.cmd");
        std::ofstream scriptFile(scriptFilePath.string());
        scriptFile << "@echo off" << std::endl;
        scriptFile << script << std::endl;
        scriptFile.close();

        std::map<std::string, std::string> env;
        env["TMP"] = tmpDir.string();

        std::filesystem::path wdir;
        auto locked = _workingDir.lock();
        if (locked != nullptr) {
            wdir = locked->absolutePath();
        } else {
            wdir = std::filesystem::current_path();
        }

        auto executor = std::make_shared<MonitoredProcess>(
            cmdExe,
            std::string("/c ") + scriptFilePath.string(),
            wdir,
            env);
        _scriptExecutor.store(executor);
        MonitoredProcessResult result = executor->wait();
        _scriptExecutor.store(nullptr);

        if (result.exitCode == 0 || canceling()) {
            std::error_code ec;
            bool removed = std::filesystem::remove_all(tmpDir, ec);
            if (!removed) {
                logDirRemovalError(this, tmpDir, ec, logBook);
            }
            result.readOnlyFiles.erase(scriptFilePath);
        } else if (!canceling()) {
            logScriptFailure(this, result, tmpDir, logBook);
        }
        return result;
    }

    bool CommandNode::findInputNodes(
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& allowedGenInputFiles,
        std::set<std::filesystem::path>const& inputSymPaths,
        std::vector<std::shared_ptr<FileNode>>& inputNodes,
        std::vector<std::shared_ptr<Node>>& srcInputNodes,
        ILogBook& logBook
    ) {
        bool allValid = true;
        auto& nodes = context()->nodes();
        for (auto const& symInputPath : inputSymPaths) {
            bool valid = true;
            auto inputNode = nodes.find(symInputPath);
            auto fileNode = dynamic_pointer_cast<FileNode>(inputNode);
            if (inputNode != nullptr && fileNode == nullptr) {
                throw std::exception("input node not a file node");
            }
            auto genInputFile = dynamic_pointer_cast<GeneratedFileNode>(fileNode);
            if (genInputFile != nullptr) {
                if (!allowedGenInputFiles.contains(symInputPath)) {
                    valid = false;
                    logBuildOrderNotGuaranteed(this, genInputFile.get(), logBook);
                } else {
                    // _inputProducers must have moved the generated inputs to Ok,
                    // Failed or Canceled state during requisite execution.
                    auto state = genInputFile->state();
                    if (
                        state != Node::State::Ok
                        && state != Node::State::Failed
                        && state != Node::State::Canceled
                    ) throw std::exception("generated input node not executed");
                    inputNodes.push_back(genInputFile);
                }
            } else {
                // In YAM all output (generated file) nodes are created
                // before commands that use them as inputs are executed.
                // Hence inputPath identifies a source file.
                auto srcInputFile = dynamic_pointer_cast<SourceFileNode>(fileNode);
                if (srcInputFile == nullptr) {
                    // Either yam does not mirror its repositories or 
                    // inputPath references a non-existing source file.
                    srcInputFile = std::make_shared<SourceFileNode>(context(), symInputPath);
                    nodes.add(srcInputFile);
                }
                inputNodes.push_back(srcInputFile);
                srcInputNodes.push_back(srcInputFile);
            }
            allValid = allValid && valid;
        }
        return allValid;
    }

    void CommandNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t CommandNode::typeId() const {
        return streamableTypeId;
    }

    /*
    * 

        InputNodes _detectedInputs;

        // The hash of the hashes of all items that, when changed, invalidate
        // the output files. Items include the script, the output files and 
        // the relevant aspects of the input files.
        XXH64_hash_t _executionHash;
    */
    void CommandNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(_inputAspectsName);
        streamer->streamVector(_cmdInputs);
        streamer->streamVector(_orderOnlyInputs);
        streamer->stream(_script);
        streamer->streamVector(_outputs);
        streamer->streamVector(_ignoredOutputs);
        NodeMapStreamer::stream(streamer, _detectedInputs);
        std::shared_ptr<DirectoryNode> wdir;
        if (streamer->writing()) {
            wdir = _workingDir.lock();
        }
        streamer->stream(wdir);
        if (streamer->reading()) {
            _workingDir = wdir;
        }
        streamer->stream(_executionHash);
    }

    void CommandNode::prepareDeserialize() {
        Node::prepareDeserialize();
        for (auto const& i : _inputProducers) i->removeObserver(this);
        for (auto const& i : _outputs) i->removeObserver(this);
        for (auto const& p : _detectedInputs) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(p.second);
            if (genFile == nullptr) p.second->removeObserver(this);
        }
        _cmdInputs.clear();
        _orderOnlyInputs.clear();
        _inputProducers.clear();
        _outputs.clear();
        _detectedInputs.clear();
        _ignoredOutputs.clear();
    }

    bool CommandNode::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!Node::restore(context, restored)) return false;
        for (auto const& node : _cmdInputs) {
            node->restore(context, restored);
        }
        for (auto const& genFileNode : _orderOnlyInputs) {
            genFileNode->restore(context, restored);
        }
        updateInputProducers();
        for (auto const& i : _outputs) {
            i->restore(context, restored);
            i->addObserver(this);
        }
        NodeMapStreamer::restore(_detectedInputs);
        for (auto const& p : _detectedInputs) {
            p.second->restore(context, restored);
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(p.second);
            if (genFile == nullptr) p.second->addObserver(this);
        }
        auto wdir = _workingDir.lock();
        if (wdir != nullptr) wdir->restore(context, restored);
        return true;
    }
}
