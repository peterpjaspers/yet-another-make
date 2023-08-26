#include "CommandNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "ExecutionContext.h"
#include "MonitoredProcess.h"
#include "FileAspectSet.h"
#include "FileSystem.h"
#include "SourceFileRepository.h"
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

    void computeInputNodesDifference(
        CommandNode::InputNodes const& in1,
        CommandNode::InputNodes const& in2,
        CommandNode::InputNodes& inBoth,
        CommandNode::InputNodes& onlyIn1,
        CommandNode::InputNodes& onlyIn2
    ) {
        for (auto const& i1 : in1) {
            auto const& path = i1.first;
            auto it = in2.find(path);
            if (it == in2.end()) {
                onlyIn1.insert({ path, i1.second });
            } else {
                inBoth.insert({ path, i1.second });
            }
        }
        for (auto const& i2 : in2) {
            auto const& path = i2.first;
            auto it = inBoth.find(path);
            if (it == inBoth.end()) {
                onlyIn2.insert({ path, i2.second });
            }
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

    void getOutputFileNodes(
        std::vector<std::shared_ptr<Node>> const& producers,
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>>& outputFiles
    ) {
        for (auto const& p : producers) {
            std::vector<std::shared_ptr<Node>> pOutputs;
            p->getOutputs(pOutputs);
            for (auto const& o : pOutputs) {
                auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(o);
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

    void logInputNotReadAllowed(
        CommandNode* cmd,
        std::string const& repoName,
        std::filesystem::path const& inputFile,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Input file not allowed to be read." << std::endl
            << "Fix: adjust exclude patterns of the repository," << std::endl
            << "or change command script to not depend on the input file." << std::endl
            << "Command: " << cmd->name().string() << std::endl
            << "Repository name: " << repoName << std::endl
            << "Input file: " << inputFile.string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
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

    void logNumberOfIllegalInputFiles(
        CommandNode* cmd, 
        unsigned int nIllegals,
        ILogBook& logBook
    ) {
        char buf[32];
        std::stringstream ss;
        ss 
            << itoa(nIllegals, buf, 10) << " illegal input files were detected during execution of command." << std::endl
            << "Command: " << cmd->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logNumberOfIllegalOutputFiles(
        CommandNode* cmd, 
        std::size_t nIllegals,
        ILogBook& logBook
    ) {
        char buf[32];
        std::stringstream ss;
        ss
            << itoa(static_cast<int>(nIllegals), buf, 10) << " illegal output files were detected during execution of command." << std::endl
            << "Command: " << cmd->name().string() << std::endl;
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
        std::vector<std::shared_ptr<GeneratedFileNode>> const& expected,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& actual,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Mismatch between declared outputs and actual outputs." << std::endl
            << "Fix: declare outputs and/or modify scripts and/or modify output file names." << std::endl
            << "Command : " << cmd->name().string() << std::endl
            << "Declared outputs: " << std::endl;
        for (auto const& n : expected) ss << "    " << n->name().string() << std::endl;
         ss << "Actual outputs  : " << std::endl;
        for (auto const& n : actual) ss << "    " << n->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }    
    
    void logNotWrittenOutputs(
        CommandNode* cmd,
        std::set<std::filesystem::path> const& notWritten,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Not all declared output files were written." << std::endl
            << "Command : " << cmd->name().string() << std::endl
            << "Not written outputs: " << std::endl;
        for (auto const& n : notWritten) ss << "    " << n.string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logNotDeclaredOutputs(
        CommandNode* cmd,
        std::set<std::filesystem::path> const& notDeclared,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Not all written files were declared as output files." << std::endl
            << "Command : " << cmd->name().string() << std::endl
            << "Not declared outputs: " << std::endl;
        for (auto const& n : notDeclared) ss << "    " << n.string() << std::endl;
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
        setOutputs(std::vector<std::shared_ptr<GeneratedFileNode>>());
        setInputProducers(std::vector<std::shared_ptr<Node>>());
    }

    void CommandNode::setInputAspectsName(std::string const& newName) {
        if (_inputAspectsName != newName) {
            _inputAspectsName = newName;
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::setOutputs(std::vector<std::shared_ptr<GeneratedFileNode>> const & newOutputs) {
        if (_outputs != newOutputs) {
            for (auto i : _outputs) i->removeDependant(this);
            _outputs = newOutputs;
            for (auto i : _outputs) i->addDependant(this);
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::setScript(std::string const& newScript) {
        if (newScript != _script) {
            _script = newScript;
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::setInputProducers(std::vector<std::shared_ptr<Node>> const& newInputProducers) {
        if (_inputProducers != newInputProducers) {
            for (auto i : _inputProducers) i->removeDependant(this);
            _inputProducers = newInputProducers;
            for (auto i : _inputProducers) i->addDependant(this);
            _executionHash = rand();
            modified(true);
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
        for (auto const& pair : _inputs) inputs.push_back(pair.second);
    }

    bool CommandNode::pendingStartSelf() const {
        bool pending = _executionHash != computeExecutionHash();
        return pending;
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

    void CommandNode::getSourceInputs(std::vector<std::shared_ptr<Node>> & sourceInputs) const {
        for (auto const& pair : _inputs) {
            auto const& node = pair.second;
            auto file = dynamic_pointer_cast<SourceFileNode>(node);
            if (file != nullptr) sourceInputs.push_back(file);
        }
    }

    void CommandNode::setInputs(ExecutionResult const& result) {
        // An input GeneratedFileNode is not a prerequisite of the CommandNode.
        // A command node therefore does not register itself as dependant of an
        // input GeneratedFileNode. Instead the command node registers itself as
        // dependent of the producer of the input GeneratedFileNode. This prevents
        // spurious callbacks to Node::handlePrerequisiteCompletion(Node* node)
        // from input GeneratedFileNodes to the command node.
        // Note: Dirty propagation in case of tampering with generated files
        // remains intact because a GeneratedFileNode propagates to its 
        // producer (who again propagates to its dependants, i.e to nodes that
        // read output files of the producer).
        // 
        for (auto const& path : result._removedInputPaths) {
            auto it = _inputs.find(path);
            if (it == _inputs.end()) throw std::exception("no such input");
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(it->second);
            if (genFile == nullptr) it->second->removeDependant(this);
            _inputs.erase(it);
        }
        for (auto const& node : result._addedInputNodes) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(node);
            if (genFile == nullptr) node->addDependant(this);
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
        if (illegals > 1) {
            logNumberOfIllegalOutputFiles(this, illegals, logBook);
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

    void CommandNode::cancelSelf() {
        std::shared_ptr<IMonitoredProcess> executor = _scriptExecutor.load();
        if (executor != nullptr) {
            executor->terminate();
        }
    }

    MonitoredProcessResult CommandNode::executeScript(MemoryLogBook& logBook) {
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
        } else if (!_canceling) {
            logScriptFailure(this, result, tmpDir, logBook);
        }
        return result;
    }

    void CommandNode::selfExecute() {
        auto result = std::make_shared<ExecutionResult>();
        if (_canceling) {
            result->_newState = Node::State::Canceled;
        } else {
            MonitoredProcessResult scriptResult = executeScript(result->_log);
            if (scriptResult.exitCode != 0) {
                result->_newState = _canceling ? Node::State::Canceled : Node::State::Failed;
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
        postSelfCompletion(result);
    }

    bool CommandNode::findInputNodes(
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& allowedGenInputFiles,
        std::set<std::filesystem::path>const& inputPaths,
        std::vector<std::shared_ptr<FileNode>>& inputNodes,
        std::vector<std::shared_ptr<Node>>& dirtyInputNodes,
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
                    // _inputProducers must have moved the generated inputs
                    // to Ok, Failed or Canceled state during prerequisite
                    // execution.
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
                    if (srcInputFile->state() == Node::State::Dirty) dirtyInputNodes.push_back(srcInputFile);
                }
            }
        }
        return valid;
    }

    void CommandNode::getPreCommitNodes(
        SelfExecutionResult& result,
        std::vector<std::shared_ptr<Node>>& preCommitNodes
    ) {
        if (result._newState == Node::State::Ok) {
            auto& cmdResult = dynamic_cast<ExecutionResult&>(result);
            // output nodes have been updated by command script, hence their
            // hashes need to be re-computed.
            for (auto& n : _outputs) {
                // bypass GeneratedFileNode override of setState to avoid
                // n to propagate Dirty state to this command node.
                n->Node::setState(Node::State::Dirty); 
                preCommitNodes.push_back(n);
            }
            std::vector<std::shared_ptr<GeneratedFileNode>> outputNodes;
            bool validOutputs = findOutputNodes(cmdResult._outputPaths, outputNodes, cmdResult._log);
            validOutputs = validOutputs && verifyOutputNodes(outputNodes, cmdResult._log);

            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> allowedGenInputFiles;
            getOutputFileNodes(_inputProducers, allowedGenInputFiles);
            std::vector<std::shared_ptr<FileNode>> notUsed1;
            std::vector<std::shared_ptr<Node>> notUsed2;
            bool validKeptInputs = findInputNodes(
                allowedGenInputFiles,
                cmdResult._keptInputPaths,
                notUsed1,
                notUsed2,
                cmdResult._log);
            bool validNewInputs = findInputNodes(
                allowedGenInputFiles,
                cmdResult._addedInputPaths,
                cmdResult._addedInputNodes,
                preCommitNodes,
                cmdResult._log);
            bool valid = validOutputs && validKeptInputs && validNewInputs;
            if (valid) {
                setInputs(cmdResult);
            } else {
                result._newState = Node::State::Failed;
            }
        }
    }

    void CommandNode::commitSelfCompletion(SelfExecutionResult const& result) {
        if (result._newState == Node::State::Ok) {
            auto const& cmdResult = dynamic_cast<ExecutionResult const&>(result);
            _executionHash = computeExecutionHash();
            modified(true);
        } else {
            ExecutionResult r;
            for (auto const& pair : _inputs) r._removedInputPaths.insert(pair.first);
            setInputs(r);
        }
        result._log.forwardTo(*(context()->logBook()));
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
        for (auto const& i : _inputProducers) i->removeDependant(this);
        for (auto const& i : _outputs) i->removeDependant(this);
        for (auto const& p : _inputs) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(p.second);
            if (genFile == nullptr) p.second->removeDependant(this);
        }
        _inputProducers.clear();
        _outputs.clear();
        _inputs.clear();
    }

    void CommandNode::restore(void* context) {
        Node::restore(context);
        for (auto const& i : _inputProducers) i->addDependant(this);
        for (auto const& i : _outputs) {
            i->addDependant(this);
            i->producer(this);
        }
        for (auto const& p : _inputs) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(p.second);
            if (genFile == nullptr) p.second->addDependant(this);
        }
    }
}
