#include "CommandNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "ExecutionContext.h"
#include "MonitoredProcess.h"
#include "FileAspectSet.h"
#include "FileSystem.h"
#include "SourceFileRepository.h"

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

    template <typename V>
    bool contains(std::vector<V> const& vec, V el) {
        return std::find(vec.begin(), vec.end(), el) != vec.end();
    }

    // Return whether 'node' is a prerequisite (recursive) of 'command'.
    // If node is a prerequisite then return the distance the between command
    // and node in 'distance'.
    bool isPrerequisite(Node* command, Node* node, std::unordered_set<Node*>& visited, int& distance)    {
        if (!visited.insert(node).second) return false; // node was already checked

        std::unordered_set<Node*> parents = node->preParents();
        if (parents.contains(command)) return true;

        distance++;
        for (auto parent : parents) {
            if (isPrerequisite(command, parent, visited, distance)) return true;
        }
        return false;
    }

    void logBuildOrderNotGuaranteed(
        CommandNode* cmd,
        GeneratedFileNode* inputFile
    ) {
        std::stringstream ss;
        ss
            << "Build order is not guaranteed." << std::endl
            << "Fix: declare input file as input of command." << std::endl
            << "Command   : " << cmd->name().string() << std::endl
            << "Input file: " << inputFile->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        cmd->context()->addToLogBook(record);
    }

    void logDiscouragedBuildOrderGuarantee(
        CommandNode* cmd,
        GeneratedFileNode* inputFile
    ) {
        std::stringstream ss;
        ss
            << "Build order guaranteed by discouraged indirect input declaration." << std::endl
            << "Fix: declare input file as direct input of command." << std::endl
            << "Command   : " << cmd->name().string() << std::endl
            << "Input file: " << inputFile->name().string() << std::endl;
        LogRecord record(LogRecord::Warning, ss.str());
        cmd->context()->addToLogBook(record);
    }

    void logInputNotInARepository(
        CommandNode* cmd,
        std::filesystem::path const& inputFile
    ) {
        std::stringstream ss;
        ss
            << "Input file not in a known file repository." << std::endl
            << "Fix: declare the file repository that contains the input," << std::endl
            << "or change command script to not depend on the input file." << std::endl
            << "Command   : " << cmd->name().string() << std::endl
            << "Input file: " << inputFile.string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        cmd->context()->addToLogBook(record);
    }

    void logInputNotReadAllowed(
        CommandNode* cmd,
        std::string const& repoName,
        std::filesystem::path const& inputFile
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
        cmd->context()->addToLogBook(record);
    }

    void logWriteAccessedSourceFile(
        CommandNode* cmd,
        SourceFileNode* outputFile
    ) {
        std::stringstream ss;
        ss
            << "Source file is updated by build." << std::endl
            << "Fix: change command script to not update the source file." << std::endl
            << "Command    : " << cmd->name().string() << std::endl
            << "Source file: " << outputFile->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        cmd->context()->addToLogBook(record);
    }
    
    void logOutputFileNotDeclared(
        CommandNode* cmd,
        std::filesystem::path const& outputFile
    ) {
        std::stringstream ss;
        ss
            << "Unknown output file."
            << "Fix: declare the file as output of command." << std::endl
            << "Command    : " << cmd->name().string() << std::endl
            << "Output file: " << outputFile.string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        cmd->context()->addToLogBook(record);
    }

    void logNumberOfIllegalInputFiles(CommandNode* cmd, unsigned int nIllegals) {
        char buf[32];
        std::stringstream ss;
        ss 
            << itoa(nIllegals, buf, 10) << " illegal input files were detected during execution of command." << std::endl
            << "Command: " << cmd->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        cmd->context()->addToLogBook(record);
    }

    void logNumberOfIllegalOutputFiles(CommandNode* cmd, unsigned int nIllegals) {
        char buf[32];
        std::stringstream ss;
        ss
            << itoa(nIllegals, buf, 10) << " illegal output files were detected during execution of command." << std::endl
            << "Command: " << cmd->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        cmd->context()->addToLogBook(record);
    }

    void logAlreadyProducedByOtherCommand(
        CommandNode* cmd,
        GeneratedFileNode* outputFile
    ) {
        std::stringstream ss;
        ss
            << "Output file is produced by 2 commands." << std::endl
            << "Fix: adapt command script to ensure that file is produced by one command only." << std::endl
            << "Command 1  : " << outputFile->producer()->name().string() << std::endl
            << "Command 2  : " << cmd->name().string() << std::endl
            << "Output file: " << outputFile->name().string() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        cmd->context()->addToLogBook(record);
    }

    void logUnexpectedOutputs(
        CommandNode* cmd,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& expected,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& actual
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
        cmd->context()->addToLogBook(record);
    }

    void logScriptFailure(
        CommandNode* cmd,
        MonitoredProcessResult const& result,
        std::filesystem::path tmpDir
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
        cmd->context()->addToLogBook(record);
    }

    void logDirRemovalError(
        CommandNode* cmd,
        ILogBook* logBook, 
        std::filesystem::path const& dir, 
        std::error_code ec
    ) {
        std::stringstream ss;
        ss
            << "Failed to delete temporary script directory." << std::endl
            << "Command : " << cmd->name().string() << std::endl
            << "Tmp dir : " << dir.string() << std::endl
            << "Reason: " << ec.message() << std::endl;
        LogRecord record(LogRecord::Error, ss.str());
        logBook->add(record);
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
            _executionHash = rand();
            setState(State::Dirty);
        }
    }

    void CommandNode::setScript(std::string const& newScript) {
        if (newScript != _script) {
            _script = newScript;
            _executionHash = rand();
            setState(State::Dirty);
        }
    }

    void CommandNode::setInputProducers(std::vector<std::shared_ptr<Node>> const& newInputProducers) {
        if (_inputProducers != newInputProducers) {
            for (auto i : _inputProducers) i->removePreParent(this);
            _inputProducers = newInputProducers;
            for (auto i : _inputProducers) i->addPreParent(this);
            _executionHash = rand();
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
        hashes.push_back(XXH64_string(_script));
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
            if (input == exclude) {
            } else {
                auto repo = context()->findRepositoryContaining(input);
                if (repo == nullptr) {
                    valid = false;
                    logInputNotInARepository(this, input);
                } else if (!repo->readAllowed(input)) {
                    valid = false;
                    logInputNotReadAllowed(this, repo->name(), input);
                }
            }
        } else {
            auto genInputFileNode = dynamic_pointer_cast<GeneratedFileNode>(fileNode);
            if (genInputFileNode != nullptr) {
                int distance = 0;
                std::unordered_set<Node*> visitedNodes;
                valid = isPrerequisite(this, genInputFileNode->producer(), visitedNodes, distance);
                if (!valid) {
                    logBuildOrderNotGuaranteed(this, genInputFileNode.get());
                } else if (distance > 0) {
                    logDiscouragedBuildOrderGuarantee(this, genInputFileNode.get());
                }
            } else if (dynamic_pointer_cast<SourceFileNode>(fileNode) == nullptr) {
                throw std::exception("Unexpected input file node type");
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
        if (illegals > 1) {
            logNumberOfIllegalInputFiles(this, illegals);
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
                logWriteAccessedSourceFile(this, srcFileNode.get());
            } else {
                valid = false;
                logOutputFileNotDeclared(this, output);
            }
        } else if (genFileNode->producer() != this) {
            valid = false;
            logAlreadyProducedByOtherCommand(this, genFileNode.get());
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
        if (illegals > 1) {
            logNumberOfIllegalOutputFiles(this, illegals);
        }
        return illegals == 0;
    }

    bool CommandNode::verifyOutputNodes(
        std::vector<std::shared_ptr<GeneratedFileNode>> const& newOutputs
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
            logUnexpectedOutputs(this, _outputs, newOutputs);
        }
        return valid;
    }

    void CommandNode::cancelSelf() {
        std::shared_ptr<IMonitoredProcess> executor = _scriptExecutor.load();
        if (executor != nullptr) {
            executor->terminate();
        }
    }

    MonitoredProcessResult CommandNode::executeScript(std::filesystem::path& scriptFilePath) {
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
                logDirRemovalError(this, context()->logBook().get(), tmpDir, ec);
            }
        } else if (!_canceling) {
            logScriptFailure(this, result, tmpDir);
        }
        return result;
    }

    void CommandNode::execute() {
        Node::State newState;
        if (_canceling) {
            newState = Node::State::Canceled;
        } else {
            std::filesystem::path scriptFilePath;
            MonitoredProcessResult result = executeScript(scriptFilePath);
            if (result.exitCode != 0) {
                if (_canceling) {
                    newState = Node::State::Canceled;
                } else {
                    newState = Node::State::Failed;
                    _executionHash = rand();
                }
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
        }
        postSelfCompletion(newState);
    }
}
