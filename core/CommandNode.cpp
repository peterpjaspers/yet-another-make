#include "CommandNode.h"
#include "DirectoryNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "ExecutionContext.h"
#include "MonitoredProcess.h"
#include "FileAspectSet.h"
#include "FileSystem.h"
#include "FileRepository.h"
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
        std::unordered_set<CommandNode*> const& producers,
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>>& outputFiles
    ) {
        for (auto const& producer : producers) {
            for (auto const& genFile : producer->outputs()) {
                 outputFiles.insert(std::pair{ genFile->name(), genFile });
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

    CommandNode::CommandNode() : Node() {}

    CommandNode::CommandNode(
        ExecutionContext* context,
        std::filesystem::path const& name)
        : Node(context, name)
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
        for (auto const& pair : _detectedInputs) {
            auto const& node = pair.second;
            if (!isGenerated(node)) node->removeObserver(this);
        }
    }

    void CommandNode::inputAspectsName(std::string const& newName) {
        if (_inputAspectsName != newName) {
            _inputAspectsName = newName;
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::cmdInputs(std::vector<std::shared_ptr<FileNode>> const& newInputs) {
        if (_cmdInputs != newInputs) {
            _cmdInputs = newInputs;
            updateInputProducers();
            _executionHash = rand();
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::orderOnlyInputs(std::vector<std::shared_ptr<GeneratedFileNode>> const& newInputs) {
        if (_orderOnlyInputs != newInputs) {
            _orderOnlyInputs = newInputs;
            updateInputProducers();
            _executionHash = rand();
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::updateInputProducers() {
        for (auto producer : _inputProducers) {
            producer->removeObserver(this);
        }
        _inputProducers.clear();
        for (auto const& node : _cmdInputs) {
            auto genFileNode = dynamic_pointer_cast<GeneratedFileNode>(node);
            if (genFileNode != nullptr) {
                _inputProducers.insert(genFileNode->producer());
            }
        }
        for (auto const& genFileNode : _orderOnlyInputs) {
            _inputProducers.insert(genFileNode->producer());
        }
        for (auto producer : _inputProducers) {
            producer->addObserver(this);
        }
    }

    void CommandNode::outputs(std::vector<std::shared_ptr<GeneratedFileNode>> const & newOutputs) {
        if (_outputs != newOutputs) {
            for (auto output : _outputs) {
                output->deleteFile();
                output->removeObserver(this);
            }
            _outputs = newOutputs;
            for (auto output : _outputs) output->addObserver(this);
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::ignoreOutputs(std::vector<std::string> const& newOutputs) {
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

    // Propagate Dirty state of input producers, outputs or sourcefile inputs
    // to this node. 
    void CommandNode::handleDirtyOf(Node* observedNode) {
        if (observedNode->state() != Node::State::Dirty) throw std::exception("observed node not dirty");
        setState(Node::State::Dirty);
    }

    XXH64_hash_t CommandNode::computeExecutionHash() const {
        std::vector<XXH64_hash_t> hashes;
        auto wdir = _workingDir.lock();
        if (wdir != nullptr) hashes.push_back(XXH64_string(wdir->name().string()));
        hashes.push_back(XXH64_string(_script));
        auto entireFile = FileAspect::entireFileAspect().name();
        for (auto node : _outputs) hashes.push_back(node->hashOf(entireFile));
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
        for (auto const& path : result._removedInputPaths) {
            auto it = _detectedInputs.find(path);
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
        modified(true);
    }

    std::shared_ptr<GeneratedFileNode> CommandNode::findOutputNode(
        std::shared_ptr<FileRepository> repo,
        std::filesystem::path const& absPath,
        MemoryLogBook& logBook
    ) {
        bool valid = true;
        std::shared_ptr<GeneratedFileNode> outputNode;
        auto symPath = repo->symbolicPathOf(absPath);
        std::shared_ptr<Node> node = context()->nodes().find(symPath);
        outputNode = dynamic_pointer_cast<GeneratedFileNode>(node);
        if (outputNode == nullptr) {
            auto srcFileNode = dynamic_pointer_cast<SourceFileNode>(node);
            if (srcFileNode != nullptr) {
                logWriteAccessedSourceFile(this, srcFileNode.get(), logBook);
            } else {
                valid = false;
                logOutputFileNotDeclared(this, absPath, logBook);
            }
        } else if (outputNode->producer() != this) {
            valid = false;
            logAlreadyProducedByOtherCommand(this, outputNode.get(), logBook);
        }
        return valid ? outputNode : nullptr;
    }

    bool CommandNode::findOutputNodes(
        std::set<std::filesystem::path> const& absOutputPaths,
        std::vector<std::shared_ptr<GeneratedFileNode>>& outputNodes,
        MemoryLogBook& logBook
    ) {
        unsigned int illegals = 0;
        for (auto const& absPath : absOutputPaths) {
            std::shared_ptr<FileRepository> const& repo = context()->findRepositoryContaining(absPath);
            if (repo != nullptr) {
                std::shared_ptr<GeneratedFileNode> fileNode = findOutputNode(repo, absPath, logBook);
                if (fileNode == nullptr) {
                    illegals++;
                } else {
                    outputNodes.push_back(fileNode);
                }
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

    // threadpool
    void CommandNode::executeScript() {
        auto result = std::make_shared<ExecutionResult>();
        if (canceling()) {
            result->_newState = Node::State::Canceled;
        } else {
            MonitoredProcessResult scriptResult = executeMonitoredScript(result->_log);
            if (scriptResult.exitCode != 0) {
                result->_newState = canceling() ? Node::State::Canceled : Node::State::Failed;
            } else {
                std::set<std::filesystem::path> previousInputPaths;
                for (auto const& pair : _detectedInputs) {
                    previousInputPaths.insert(pair.second->absolutePath());
                }
                std::set<std::filesystem::path>& currentInputPaths = scriptResult.readOnlyFiles;
                computePathSetsDifference(
                    previousInputPaths, currentInputPaths,
                    result->_keptInputPaths,
                    result->_removedInputPaths,
                    result->_addedInputPaths);
                result->_outputPaths = scriptResult.writtenFiles;
                result->_newState = Node::State::Ok;
            }
        }
        auto d = Delegate<void>::CreateLambda(
            [this, result]() { handleExecuteScriptCompletion(*result); }
        );
        context()->mainThreadQueue().push(std::move(d));
    }

    void CommandNode::handleExecuteScriptCompletion(ExecutionResult& result) {
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
                // Kept inputs must be validated because of possible change in file
                // repository configuration.
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
                        [this](Node::State s) { handleOutputAndNewInputFilesCompletion(s); }
                    );
                    startNodes(rawNodes, std::move(callback));
                } else {
                    result._newState = Node::State::Failed;
                }
            }
        }
        result._log.forwardTo(*(context()->logBook()));
        if (result._newState != Node::State::Ok) {
            notifyCompletion(result._newState);
        }
    }

    void CommandNode::handleOutputAndNewInputFilesCompletion(Node::State state) {
        if (state == Node::State::Ok) {
            _executionHash = computeExecutionHash();
            modified(true);
        } else {
            ExecutionResult r;
            for (auto const& pair : _detectedInputs) r._removedInputPaths.insert(pair.first);
            setDetectedInputs(r);
        }
        notifyCompletion(state);
    }

    // threadpool
    MonitoredProcessResult CommandNode::executeMonitoredScript(MemoryLogBook& logBook) {
        if (_script.empty()) return MonitoredProcessResult();

        std::filesystem::path tmpDir = FileSystem::createUniqueDirectory();
        auto scriptFilePath = std::filesystem::path(tmpDir / "cmdscript.cmd");
        std::ofstream scriptFile(scriptFilePath.string());
        scriptFile << _script << std::endl;
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
        std::set<std::filesystem::path>const& absInputPaths,
        std::vector<std::shared_ptr<FileNode>>& inputNodes,
        std::vector<std::shared_ptr<Node>>& srcInputNodes,
        ILogBook& logBook
    ) {
        bool allValid = true;
        auto& nodes = context()->nodes();
        for (auto const& absInputPath : absInputPaths) {
            bool valid = true;
            auto repo = context()->findRepositoryContaining(absInputPath);
            valid = repo != nullptr;
            if (valid) {
                auto symInputPath = repo->symbolicPathOf(absInputPath);
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

    void CommandNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->streamVector(_cmdInputs);
        streamer->streamVector(_orderOnlyInputs);

        std::vector<std::shared_ptr<FileNode>> inputs;
        std::shared_ptr<DirectoryNode> wdir;
        if (streamer->writing()) {
            for (auto const& pair : _detectedInputs) inputs.push_back(pair.second);
            wdir = _workingDir.lock();
        }
        streamer->streamVector(inputs);
        streamer->stream(wdir);
        if (streamer->reading()) {
            for (auto const& n : inputs) _detectedInputs.insert({ n->name(), n });
            _workingDir = wdir;
        }
        streamer->stream(_script);
        streamer->streamVector(_outputs);
        streamer->streamVector(_ignoredOutputs);
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

    void CommandNode::restore(void* context) {
        Node::restore(context);
        updateInputProducers();
        for (auto const& i : _outputs) {
            i->addObserver(this);
            i->producer(this);
        }
        for (auto const& p : _detectedInputs) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(p.second);
            if (genFile == nullptr) p.second->addObserver(this);
        }
    }
}
