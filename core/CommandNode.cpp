#include "CommandNode.h"
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

    void getOutputFileNodes(
        std::vector<std::shared_ptr<Node>> const& producers,
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>>& outputFiles
    ) {
        for (auto const& p : producers) {
            std::vector<std::shared_ptr<Node>> pOutputs;
            p->getOutputs(pOutputs);
            for (auto const& n : pOutputs) {
                auto genFile = dynamic_pointer_cast<GeneratedFileNode>(n);
                if (genFile != nullptr) {
                    outputFiles.insert(std::pair{ genFile->name(), genFile });
                }
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
            << "Input file: " << inputFile->name().string() << std::endl;
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
        // Clearing these collections is needed to stop observing their
        // elements by this command node. 
        outputs(std::vector<std::shared_ptr<GeneratedFileNode>>());
        inputProducers(std::vector<std::shared_ptr<Node>>());
        ExecutionResult clear;
        for (auto const& pair : _inputs) clear._removedInputPaths.insert(pair.first);
        setInputs(clear);
    }

    void CommandNode::inputAspectsName(std::string const& newName) {
        if (_inputAspectsName != newName) {
            _inputAspectsName = newName;
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::outputs(std::vector<std::shared_ptr<GeneratedFileNode>> const & newOutputs) {
        if (_outputs != newOutputs) {
            for (auto i : _outputs) i->removeObserver(this);
            _outputs = newOutputs;
            for (auto i : _outputs) i->addObserver(this);
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

    void CommandNode::inputProducers(std::vector<std::shared_ptr<Node>> const& newInputProducers) {
        if (_inputProducers != newInputProducers) {
            for (auto i : _inputProducers) i->removeObserver(this);
            _inputProducers = newInputProducers;
            for (auto i : _inputProducers) i->addObserver(this);
            _executionHash = rand();
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        for (auto const& n : _outputs) outputs.push_back(dynamic_pointer_cast<Node>(n));
    }

    void CommandNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
        for (auto const& pair : _inputs) inputs.push_back(dynamic_pointer_cast<Node>(pair.second));
    }

    // Propagate Dirty state of input producers, outputs or sourcefile inputs
    // to this node. 
    void CommandNode::handleDirtyOf(Node* observedNode) {
        if (observedNode->state() != Node::State::Dirty) throw std::exception("observed node not dirty");
        setState(Node::State::Dirty);
    }

    XXH64_hash_t CommandNode::computeExecutionHash() const {
        std::vector<XXH64_hash_t> hashes;
        hashes.push_back(XXH64_string(_script));
        auto entireFile = FileAspect::entireFileAspect().name();
        for (auto node : _outputs) hashes.push_back(node->hashOf(entireFile));
        FileAspectSet inputAspects = context()->findFileAspectSet(_inputAspectsName);
        for (auto const& pair : _inputs) {
            auto const& node = pair.second;
            auto& inputAspect = inputAspects.findApplicableAspect(node->name());
            hashes.push_back(node->hashOf(inputAspect.name()));
        }
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    void CommandNode::getSourceInputs(std::vector<Node*>& sourceInputs) const {
        for (auto const& pair : _inputs) {
            auto const& node = pair.second;
            auto file = dynamic_pointer_cast<SourceFileNode>(node);
            if (file != nullptr) sourceInputs.push_back(file.get());
        }
    }

    void CommandNode::setInputs(ExecutionResult const& result) {
        // Note that the producer of an input GeneratedFileNode is a requisite
        // of the command node, not the GeneratedFileNode itself. 
        // A command node therefore does not register itself as observer of an
        // input GeneratedFileNode. Instead it registers itself as observer of
        // the producer of the input GeneratedFileNode (in function 
        // inputProducers(..)). This prevents a spurious callback to 
        // Node::handleCompletionOf(Node* genFileNode) from the input
        // GeneratedFileNode to the command node.
        // Note: Dirty propagation in case of tampering with generated files
        // remains intact because a GeneratedFileNode propagates Dirty to its 
        // producer (who then notifies its observers, i.e to nodes that read
        // one or more output files of the producer).
        // 
        for (auto const& path : result._removedInputPaths) {
            auto it = _inputs.find(path);
            if (it == _inputs.end()) throw std::exception("no such input");
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(it->second);
            if (genFile == nullptr) it->second->removeObserver(this);
            _inputs.erase(it);
        }
        for (auto const& node : result._addedInputNodes) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(node);
            if (genFile == nullptr) node->addObserver(this);
            auto result = _inputs.insert({ node->name(), node });
            if (!result.second) throw std::exception("attempt to add duplicate input");
        }
        modified(true);
    }

    std::shared_ptr<GeneratedFileNode> CommandNode::findOutputNode(
        std::filesystem::path const& output,
        MemoryLogBook& logBook
    ) {
        bool valid = true;
        std::shared_ptr<Node> node = context()->nodes().find(output);
        auto genFileNode = dynamic_pointer_cast<GeneratedFileNode>(node);
        if (genFileNode == nullptr) {
            auto srcFileNode = dynamic_pointer_cast<SourceFileNode>(node);
            if (srcFileNode != nullptr) {
                logWriteAccessedSourceFile(this, srcFileNode.get(), logBook);
            } else {
                valid = false;
                logOutputFileNotDeclared(this, output, logBook);
            }
        } else if (genFileNode->producer() != this) {
            valid = false;
            logAlreadyProducedByOtherCommand(this, genFileNode.get(), logBook);
        }
        return valid ? genFileNode : nullptr;
    }

    bool CommandNode::findOutputNodes(
        std::set<std::filesystem::path> const& outputPaths,
        std::vector<std::shared_ptr<GeneratedFileNode>>& outputNodes,
        MemoryLogBook& logBook
    ) {
        unsigned int illegals = 0;
        for (auto const& path : outputPaths) {
            std::shared_ptr<GeneratedFileNode> fileNode = findOutputNode(path, logBook);
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
            }
        }
    }

    void CommandNode::start() {
        Node::start();
        std::vector<Node*> requisites;
        for (auto const& p : _inputProducers) requisites.push_back(p.get());
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

    void CommandNode::executeScript() {
        auto result = std::make_shared<ExecutionResult>();
        if (canceling()) {
            result->_newState = Node::State::Canceled;
        } else {
            MonitoredProcessResult scriptResult = executeMonitoredScript(result->_log);
            if (scriptResult.exitCode != 0) {
                result->_newState = canceling() ? Node::State::Canceled : Node::State::Failed;
            } else {
                std::set<std::filesystem::path> inputPaths;
                for (auto const& pair : _inputs) inputPaths.insert(pair.first);
                computePathSetsDifference(
                    inputPaths, scriptResult.readOnlyFiles,
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
                    setInputs(result);
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
            for (auto const& pair : _inputs) r._removedInputPaths.insert(pair.first);
            setInputs(r);
        }
        notifyCompletion(state);
    }

    MonitoredProcessResult CommandNode::executeMonitoredScript(MemoryLogBook& logBook) {
        if (_script.empty()) return MonitoredProcessResult();

        std::filesystem::path tmpDir = FileSystem::createUniqueDirectory();
        auto scriptFilePath = std::filesystem::path(tmpDir / "cmdscript.cmd");
        std::ofstream scriptFile(scriptFilePath.string());
        scriptFile << _script << std::endl;
        scriptFile.close();

        std::map<std::string, std::string> env;
        env["TMP"] = tmpDir.string();

        auto executor = std::make_shared<MonitoredProcess>(
            cmdExe,
            std::string("/c ") + scriptFilePath.string(),
            env);
        _scriptExecutor = executor;
        MonitoredProcessResult result = executor->wait();
        _scriptExecutor.store(nullptr);

        if (result.exitCode == 0) {
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
        std::set<std::filesystem::path>const& inputPaths,
        std::vector<std::shared_ptr<FileNode>>& inputNodes,
        std::vector<std::shared_ptr<Node>>& srcInputNodes,
        ILogBook& logBook
    ) {
        bool valid = true;
        auto& nodes = context()->nodes();
        for (auto const& inputPath : inputPaths) {
            auto inputNode = nodes.find(inputPath);
            auto fileNode = dynamic_pointer_cast<FileNode>(inputNode);
            if (inputNode != nullptr && fileNode == nullptr) {
                throw std::exception("input node not a file node");
            }
            auto generatedInputFile = dynamic_pointer_cast<GeneratedFileNode>(fileNode);
            if (generatedInputFile != nullptr) {
                if (!allowedGenInputFiles.contains(inputPath)) {
                    valid = false;
                    logBuildOrderNotGuaranteed(this, generatedInputFile.get(), logBook);
                } else {
                    // _inputProducers must have moved the generated inputs to Ok,
                    // Failed or Canceled state during requisite execution.
                    auto state = generatedInputFile->state();
                    if (
                        state != Node::State::Ok
                        && state != Node::State::Failed
                        && state != Node::State::Canceled
                    ) throw std::exception("generated input node not executed");
                    inputNodes.push_back(generatedInputFile);
                }
            } else {
                auto repo = context()->findRepositoryContaining(inputPath);
                if (repo == nullptr) {
                    logInputNotInARepository(this, inputPath, logBook);
                } else {
                    // In YAM all output (generated file) nodes must be created
                    // before command execution starts. Hence when inputPath is
                    // not a generated file it must be a source file.
                    auto srcInputFile = dynamic_pointer_cast<SourceFileNode>(fileNode);
                    if (srcInputFile == nullptr) {
                        srcInputFile = std::make_shared<SourceFileNode>(context(), inputPath);
                        nodes.add(srcInputFile);
                    }
                    inputNodes.push_back(srcInputFile);
                    srcInputNodes.push_back(srcInputFile);
                }
            }
        }
        return valid;
    }

    void CommandNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t CommandNode::typeId() const {
        return streamableTypeId;
    }

    void CommandNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->streamVector(_inputProducers);
        streamer->streamVector(_outputs);
        std::vector<std::shared_ptr<FileNode>> inputs;
        if (streamer->writing()) {
            for (auto const& pair : _inputs) inputs.push_back(pair.second);
        }
        streamer->streamVector(inputs);
        if (streamer->reading()) {
            for (auto const& n : inputs) _inputs.insert({ n->name(), n });
        }
        streamer->stream(_script);
        streamer->stream(_executionHash);
    }

    void CommandNode::prepareDeserialize() {
        Node::prepareDeserialize();
        for (auto const& i : _inputProducers) i->removeObserver(this);
        for (auto const& i : _outputs) i->removeObserver(this);
        for (auto const& p : _inputs) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(p.second);
            if (genFile == nullptr) p.second->removeObserver(this);
        }
        _inputProducers.clear();
        _outputs.clear();
        _inputs.clear();
    }

    void CommandNode::restore(void* context) {
        Node::restore(context);
        for (auto const& i : _inputProducers) i->addObserver(this);
        for (auto const& i : _outputs) {
            i->addObserver(this);
            i->producer(this);
        }
        for (auto const& p : _inputs) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(p.second);
            if (genFile == nullptr) p.second->addObserver(this);
        }
    }
}
