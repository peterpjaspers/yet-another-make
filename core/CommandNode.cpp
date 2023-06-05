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

    void computeSetDifference(
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

    void logBuildOrderNotGuaranteed(
        CommandNode* cmd,
        GeneratedFileNode* inputFile,
        MemoryLogBook& logBook
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
        MemoryLogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Input file not in a known file repository." << std::endl
            << "Fix: declare the file repository that contains the input," << std::endl
            << "or change command script to not depend on the input file." << std::endl
            << "Command   : " << cmd->name().string() << std::endl
            << "Input file: " << inputFile.string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logInputNotReadAllowed(
        CommandNode* cmd,
        std::string const& repoName,
        std::filesystem::path const& inputFile,
        MemoryLogBook& logBook
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
        MemoryLogBook& logBook
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
        MemoryLogBook& logBook
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
        MemoryLogBook& logBook
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
        unsigned int nIllegals,
        MemoryLogBook& logBook
    ) {
        char buf[32];
        std::stringstream ss;
        ss
            << itoa(nIllegals, buf, 10) << " illegal output files were detected during execution of command." << std::endl
            << "Command: " << cmd->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logAlreadyProducedByOtherCommand(
        CommandNode* cmd,
        GeneratedFileNode* outputFile,
        MemoryLogBook& logBook
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
        MemoryLogBook& logBook
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

    void logScriptFailure(
        CommandNode* cmd,
        MonitoredProcessResult const& result,
        std::filesystem::path tmpDir,
        MemoryLogBook& logBook
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
        MemoryLogBook& logBook
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
            modified(true);
            setState(State::Dirty);
        }
    }

    void CommandNode::setOutputs(std::vector<std::shared_ptr<GeneratedFileNode>> const & newOutputs) {
        if (_outputs != newOutputs) {
            for (auto i : _outputs) i->removePreParent(this);
            _outputs = newOutputs;
            for (auto i : _outputs) i->addPreParent(this);
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
            for (auto i : _inputProducers) i->removePreParent(this);
            _inputProducers = newInputProducers;
            for (auto i : _inputProducers) i->addPreParent(this);
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

    XXH64_hash_t CommandNode::computeExecutionHash(ExecutionResult const* result) const {
        std::vector<XXH64_hash_t> hashes;
        hashes.push_back(XXH64_string(_script));
        auto entireFile = FileAspect::entireFileAspect().name();
        for (auto r : result->_outputNodeResults) hashes.push_back(r->hashOf(entireFile));
        FileAspectSet inputAspects = context()->findFileAspectSet(_inputAspectsName);
        for (auto const& pair : result->_inputs) {
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

    bool CommandNode::executeOutputFiles(
        std::vector<std::shared_ptr<FileNode::ExecutionResult>> & outputHashes) {
        for (auto ofile : _outputs) {
            auto result = std::make_shared< FileNode::ExecutionResult>();
            ofile->selfExecute(result.get());
            if (result->_newState != Node::State::Ok) return(false);
            outputHashes.push_back(result);
        }
        return true;
    }

    void CommandNode::setInputs(ExecutionResult const* result) {
        for (auto const& pair : result->_removedInputs) {
            pair.second->removePreParent(this);
            _inputs.erase(pair.first);
        }
        for (auto const& pair : result->_addedInputs) {
            pair.second->addPreParent(this);
            _inputs.insert(pair);
        }
        modified(true);
    }

    std::shared_ptr<FileNode> CommandNode::findInputNode(
        std::filesystem::path const& input,
        MemoryLogBook& logBook
    ) {
        bool valid = true;
        auto node = context()->nodes().find(input);
        auto fileNode = dynamic_pointer_cast<FileNode>(node);
        if (fileNode == nullptr) {
            auto repo = context()->findRepositoryContaining(input);
            if (repo == nullptr) {
                valid = false;
                logInputNotInARepository(this, input, logBook);
            } else if (!repo->readAllowed(input)) {
                valid = false;
                logInputNotReadAllowed(this, repo->name(), input, logBook);
            }
        } else {
            auto genInputFileNode = dynamic_pointer_cast<GeneratedFileNode>(fileNode);
            if (genInputFileNode != nullptr) {
                valid = false;
                for (auto const& ip : _inputProducers) {
                    valid = (ip.get() == genInputFileNode->producer());
                    if (valid) break;
                }
                if (!valid) {
                    logBuildOrderNotGuaranteed(this, genInputFileNode.get(), logBook);
                }
            } else if (dynamic_pointer_cast<SourceFileNode>(fileNode) == nullptr) {
                throw std::exception("Unexpected input file node type");
            }
        }
        return valid ? fileNode : nullptr;
    }

    bool CommandNode::findInputNodes(
        std::set<std::filesystem::path> const& inputFilePaths,
        std::filesystem::path const& exclude,
        ExecutionResult* result
    ) {
        unsigned int illegals = 0;
        for (auto const& path : inputFilePaths) {
            if (path != exclude) {
                std::shared_ptr<FileNode> fileNode = findInputNode(path, result->_log);
                if (fileNode == nullptr) {
                    illegals++;
                } else {
                    result->_inputs.insert({ path, fileNode });
                }
            }
        }
        if (illegals == 0) {
            computeSetDifference(
                _inputs, result->_inputs,
                result->_keptInputs,
                result->_removedInputs,
                result->_addedInputs
            );
        } else {
            logNumberOfIllegalInputFiles(this, illegals, result->_log);
        }
        return illegals == 0;
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
        MonitoredProcessResult const& result,
        std::vector<std::shared_ptr<GeneratedFileNode>>& outputNodes,
            MemoryLogBook& logBook
    ) {
        unsigned int illegals = 0;
        for (auto const& path : result.writtenFiles) {
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

    MonitoredProcessResult CommandNode::executeScript(
        std::filesystem::path& scriptFilePath,
        MemoryLogBook& logBook
    ) {
        if (_script.empty()) return MonitoredProcessResult();

        std::filesystem::path tmpDir = FileSystem::createUniqueDirectory();
        scriptFilePath = std::filesystem::path(tmpDir / "cmdscript.cmd");
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
            std::filesystem::path scriptFilePath;
            MonitoredProcessResult scriptResult = executeScript(scriptFilePath, result->_log);
            if (scriptResult.exitCode != 0) {
                result->_newState = _canceling ? Node::State::Canceled : Node::State::Failed;
            } else {
                bool validInputs = findInputNodes(scriptResult.readOnlyFiles, scriptFilePath, result.get());

                std::vector<std::shared_ptr<GeneratedFileNode>> newOutputs;
                bool validOutputs = findOutputNodes(scriptResult, newOutputs, result->_log);
                validOutputs = validOutputs && verifyOutputNodes(newOutputs, result->_log);

                result->_newState = (validInputs && validOutputs) ? Node::State::Ok : Node::State::Failed;
                if (result->_newState == Node::State::Ok) {
                    if (!executeOutputFiles(result->_outputNodeResults)) {
                        result->_newState = Node::State::Failed;
                    } else {
                        result->_executionHash = computeExecutionHash(result.get());
                    }
                }
            }
        }
        postSelfCompletion(result);
    }

    void CommandNode::commitSelfCompletion(SelfExecutionResult const* result) {
        if (result->_newState == Node::State::Ok) {
            auto cmdResult = dynamic_cast<ExecutionResult const*>(result);
            if (_outputs.size() != cmdResult->_outputNodeResults.size()) {
                throw std::exception("size mismatch");
            }
            for (std::size_t i = 0; i < _outputs.size(); ++i) {
                _outputs[i]->commitSelfCompletion(cmdResult->_outputNodeResults[i].get());
            }
            setInputs(cmdResult);
            _executionHash = cmdResult->_executionHash;
            modified(true);
        }
        result->_log.forwardTo(*(context()->logBook()));
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
        for (auto const& i : _inputProducers) i->removePreParent(this);
        for (auto const& i : _outputs) i->removePreParent(this);
        for (auto const& p : _inputs) p.second->removePreParent(this);
        _inputProducers.clear();
        _outputs.clear();
        _inputs.clear();
    }

    void CommandNode::restore(void* context) {
        Node::restore(context);
        for (auto const& i : _inputProducers) i->addPreParent(this);
        for (auto const& i : _outputs) {
            i->addPreParent(this);
            i->producer(this);
        }
        for (auto const& p : _inputs) p.second->addPreParent(this);
    }
}
