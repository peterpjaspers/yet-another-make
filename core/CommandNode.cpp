#include "CommandNode.h"
#include "DirectoryNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "GroupNode.h"
#include "ExecutionContext.h"
#include "MonitoredProcess.h"
#include "FileAspectSet.h"
#include "FileSystem.h"
#include "FileRepositoryNode.h"
#include "PercentageFlagsCompiler.h"
#include "NodeMapStreamer.h"
#include "IStreamer.h"
#include "Globber.h"
#include "computeMapsDifference.h"

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

    std::vector<std::shared_ptr<Node>> getFileNodes(std::vector<std::shared_ptr<Node>> const& nodes) {
        std::vector<std::shared_ptr<Node>> files;
        for (auto const& node : nodes) {
            auto groupNode = dynamic_pointer_cast<GroupNode>(node);
            if (groupNode != nullptr) {
                auto const& content = groupNode->files();
                files.insert(files.end(), content.begin(), content.end());
            } else {
                auto fileNode = dynamic_pointer_cast<FileNode>(node);
                if (fileNode != nullptr) files.push_back(node);
            }
        }
        return files;
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
        std::unordered_set<std::shared_ptr<Node>> const& producers,
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>>& outputFiles
    ) {
        for (auto const& producer : producers) {
            auto const &cmd = dynamic_pointer_cast<CommandNode>(producer);
            if (cmd != nullptr) {
                std::vector<std::shared_ptr<GeneratedFileNode>> outputs = cmd->detectedOutputs();
                for (auto const& output : outputs) {
                    outputFiles.insert({ output->name(), output });
                }
            } else {
                auto const &group = dynamic_pointer_cast<GroupNode>(producer);
                if (group == nullptr) throw std::exception("not supported producer type");
                std::set<std::shared_ptr<FileNode>, Node::CompareName> files = group->files();
                for (auto const& file : files) {
                    auto genFileNode = dynamic_pointer_cast<GeneratedFileNode>(file);
                    if (genFileNode != nullptr) {
                        outputFiles.insert({ genFileNode->name(), genFileNode });
                    }
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
            << "Input file: " << inputFile->absolutePath().string() << std::endl;
        if (cmd->buildFile() != nullptr) {
            ss << "The command is defined in buildfile " << cmd->buildFile()->absolutePath()
               << " by the rule at line " << cmd->ruleLineNr() << std::endl;
        }
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
        if (cmd->buildFile() != nullptr) {
            ss << "The command is defined in buildfile " << cmd->buildFile()->absolutePath()
               << " by the rule at line " << cmd->ruleLineNr() << std::endl;
        }
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
        if (cmd->buildFile() != nullptr) {
            ss << "The command is defined in buildfile " << cmd->buildFile()->absolutePath()
                << " by the rule at line " << cmd->ruleLineNr() << std::endl;
        }
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    bool deleteFile(std::filesystem::path const& absPath) {
        std::error_code ec;
        std::filesystem::remove(absPath, ec);
        return !std::filesystem::exists(absPath);
    }

    void deleteNotDeclaredOutput(
        CommandNode* cmd,
        std::filesystem::path const& outputPath,
        ILogBook& logBook
    ) {
        auto repo = cmd->context()->findRepositoryContaining(outputPath);
        auto absPath = repo->absolutePathOf(outputPath);
        bool deleted = deleteFile(absPath);
        std::stringstream ss;
        if (!deleted) {
            ss << "Failed to delete not-declared output file " << absPath << std::endl;
        } else {
            ss << "Deleted not-declared output file " << absPath << std::endl;
        }
        LogRecord deleteRecord(LogRecord::Error, ss.str());
        logBook.add(deleteRecord);
    }

    void deleteIgnoredOutput(
        CommandNode* cmd,
        std::filesystem::path const& outputPath,
        ILogBook& logBook
    ) {
        auto repo = cmd->context()->findRepositoryContaining(outputPath);
        auto absPath = repo->absolutePathOf(outputPath);
        if (std::filesystem::exists(absPath)) {
            bool deleted = deleteFile(absPath);
            std::stringstream ss;
            if (!deleted) {
                ss << "Failed to delete ignored output file " << absPath << std::endl;
            } else {
                ss << "Deleted ignored output file " << absPath << std::endl;
            }
            LogRecord deleteRecord(LogRecord::Progress, ss.str());
            logBook.add(deleteRecord);
        }
    }

    void logNotDeclaredOutput(
        CommandNode* cmd,
        std::filesystem::path const &outputPath,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Not-declared output file." << std::endl
            << "Fix: declare the file as mandatory, optional or ignored output of command." << std::endl
            << "Command    : " << cmd->name().string() << std::endl
            << "Output file: " << outputPath.string() << std::endl;
        if (cmd->buildFile() != nullptr) {
            ss << "The command is defined in buildfile " << cmd->buildFile()->absolutePath()
                << " by the rule at line " << cmd->ruleLineNr() << std::endl;
        }
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logAlreadyProducedByOtherCommand(
        CommandNode* cmd,
        GeneratedFileNode* outputNode,
        CommandNode::OutputFilter::Type type,
        ILogBook& logBook
    ) {
        std::string typeStr;
        switch (type) {
        case CommandNode::OutputFilter::Type::Output:
        case CommandNode::OutputFilter::Type::ExtraOutput:
                typeStr = "Mandatory"; 
                break;
            case CommandNode::OutputFilter::Type::Optional:
                typeStr = "Optional";
                break;
            case CommandNode::OutputFilter::Type::Ignore:
                typeStr = "Ignore";
                break;
            case CommandNode::OutputFilter::Type::None:
                typeStr = "Not-declared";
                break;
            default: throw std::exception("illegal filter type");
        }
        std::stringstream ss;
        ss
            << typeStr << " output file is produced by 2 commands." << std::endl
            << "Fix: adapt command script to ensure that file is produced by one command only." << std::endl
            << "Command 1  : " << outputNode->producer()->name().string() << std::endl
            << "Command 2  : " << cmd->name().string() << std::endl
            << "Output file: " << outputNode->name().string() << std::endl;
        if (cmd->buildFile() != nullptr) {
            ss << "The command is defined in buildfile " << cmd->buildFile()->absolutePath()
                << " by the rule at line " << cmd->ruleLineNr() << std::endl;
        }
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }

    void logUnexpectedOutputs(
        CommandNode* cmd,
        std::set<std::filesystem::path> const& declared,
        std::set<std::filesystem::path> const& actual,
        ILogBook& logBook
    ) {
        std::stringstream ss;
        ss
            << "Declared mandatory outputs do not match actual outputs." << std::endl
            << "Fix: declare outputs and/or modify scripts and/or modify output file names." << std::endl
            << "Command : " << cmd->name().string() << std::endl
            << "Declared outputs: " << std::endl;
        for (auto const& path : declared) ss << "    " << path.string() << std::endl;
        ss << "Actual outputs  : " << std::endl;
        for (auto const& path : actual) ss << "    " << path.string() << std::endl;
        if (cmd->buildFile() != nullptr) {
            ss << "The command is defined in buildfile " << cmd->buildFile()->absolutePath()
                << " by the rule at line " << cmd->ruleLineNr() << std::endl;
        }
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
        if (cmd->buildFile() != nullptr) {
            ss << "The command is defined in buildfile " << cmd->buildFile()->absolutePath()
                << " by the rule at line " << cmd->ruleLineNr() << std::endl;
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
        if (cmd->buildFile() != nullptr) {
            ss << "The command is defined in buildfile " << cmd->buildFile()->absolutePath()
                << " by the rule at line " << cmd->ruleLineNr() << std::endl;
        }
        LogRecord record(LogRecord::Error, ss.str());
        logBook.add(record);
    }
}

namespace YAM
{
    using namespace boost::process;
    CommandNode::OutputFilter::OutputFilter(
        CommandNode::OutputFilter::Type type, 
        std::filesystem::path const& path)
        : _type(type) , _path(path)
    {}

    bool CommandNode::OutputFilter::operator==(OutputFilter const& rhs) const {
        return _type == rhs._type && _path == rhs._path;
    }

    void CommandNode::OutputFilter::stream(IStreamer* streamer) {
        uint32_t t;
        if (streamer->writing()) {
            t = static_cast<uint32_t>(_type);
        }
        streamer->stream(t);
        streamer->stream(_path);
        if (streamer->reading()) {
            _type = static_cast<OutputFilter::Type>(t);
        }
    }

    void CommandNode::OutputFilter::streamVector(
        IStreamer* streamer,
        std::vector<OutputFilter>& filters
    ) {
        uint32_t cnt;
        if (streamer->writing()) cnt = static_cast<uint32_t>(filters.size());
        streamer->stream(cnt);
        if (streamer->reading()) filters = std::vector<OutputFilter>(cnt);
        for (uint32_t i = 0; i < cnt; i++) filters[i].stream(streamer);
    }

    CommandNode::OutputNameFilter::OutputNameFilter(
        OutputFilter::Type type,
        std::filesystem::path const& symPath
    )
        : _type(type)
        , _symPath(symPath)
        , _glob(symPath)
    {}

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
        destroy(false);
    }

    void CommandNode::destroy(bool removeFromContext) {
        _inputAspectsName.clear();
        _cmdInputs.clear();
        _orderOnlyInputs.clear();
        for (auto const& producer : _inputProducers) {
            producer->removeObserver(this);
        }
        _inputProducers.clear();
        _script.clear();
        for (auto const& pair : _mandatoryOutputs) {
            pair.second->removeObserver(this);
        }
        _mandatoryOutputs.clear();
        _outputFilters.clear();
        _outputNameFilters.clear();
        for (auto const& pair : _detectedOptionalOutputs) {
            pair.second->removeObserver(this);
            if (removeFromContext) {
                pair.second->deleteFile(false, true);
                context()->nodes().remove(pair.second);
                DirectoryNode::removeGeneratedFile(pair.second);
            }
        }
        _detectedOptionalOutputs.clear();
        clearDetectedInputs();
    }
    
    // CommandNode is removed from context: clear members variables
    void CommandNode::cleanup() {
        destroy(true);
    }

    void CommandNode::inputAspectsName(std::string const& newName) {
        if (_inputAspectsName != newName) {
            _inputAspectsName = newName;
            modified(true);
            setState(State::Dirty);
        }
    }
    std::string const& CommandNode::inputAspectsName(std::string const& newName) const {
        return _inputAspectsName;
    }

    void CommandNode::cmdInputs(std::vector<std::shared_ptr<Node>> const& newInputs) {
        if (_cmdInputs != newInputs) {
            _cmdInputs = newInputs;
            updateInputProducers();
            clearDetectedInputs();
            modified(true);
            setState(State::Dirty);
        }
    }
    std::vector<std::shared_ptr<Node>> const& CommandNode::cmdInputs() const { 
        return _cmdInputs; 
    }

    void CommandNode::orderOnlyInputs(std::vector<std::shared_ptr<Node>> const& newInputs) {
        if (_orderOnlyInputs != newInputs) {
            _orderOnlyInputs = newInputs;
            updateInputProducers();
            clearDetectedInputs();
            modified(true);
            setState(State::Dirty);
        }
    }
    std::vector<std::shared_ptr<Node>> const& CommandNode::orderOnlyInputs() const { 
        return _orderOnlyInputs; 
    }

    void appendInputProducers(
        std::vector<std::shared_ptr<Node>>const& inputs,
        std::unordered_set<std::shared_ptr<Node>>& inputProducers
    ) {
        for (auto const& node : inputs) {
            auto genFileNode = dynamic_pointer_cast<GeneratedFileNode>(node);
            if (genFileNode != nullptr) {
                inputProducers.insert(genFileNode->producer());
            } else {
                auto groupNode = dynamic_pointer_cast<GroupNode>(node);
                if (groupNode != nullptr) inputProducers.insert(groupNode);
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

    void CommandNode::script(std::string const& newScript) {
        if (newScript != _script) {
            _script = newScript;
            clearDetectedInputs();
            modified(true);
            setState(State::Dirty);
        }
    }
    std::string const& CommandNode::script() const {
        return _script;
    }

    void CommandNode::workingDirectory(std::shared_ptr<DirectoryNode> const& dir) {
        if (_workingDir.lock() != dir) {
            _workingDir = dir;
            clearDetectedInputs();
            modified(true);
            setState(State::Dirty);
        }
    }
    std::shared_ptr<DirectoryNode> CommandNode::workingDirectory() const {
        auto wdir = _workingDir.lock();
        if (wdir == nullptr) {
            auto repo = repository();
            if (repo != nullptr) wdir = repo->directoryNode();
        }
        return wdir;
    }

    void CommandNode::outputFilters(
        std::vector<CommandNode::OutputFilter> const& newFilters,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& mandatoryOutputs
    ) {
        if (_outputFilters != newFilters) {
            _outputFilters = newFilters;
            _outputNameFilters.clear();
            updateMandatoryOutputs(mandatoryOutputs);
            clearOptionalOutputs();
            modified(true);
            setState(State::Dirty);
        }
    }

    std::vector<CommandNode::OutputFilter> const& CommandNode::outputFilters() const {
        return _outputFilters;
    }

    std::vector<CommandNode::OutputNameFilter> const& CommandNode::outputNameFilters() {
        if (_outputNameFilters.size() != _outputFilters.size()) {
            updateOutputNameFilters();
        }
        return _outputNameFilters;
    }

    void CommandNode::updateOutputNameFilters() {
        _outputNameFilters.clear();
        for (auto const& filter : _outputFilters) {
            _outputNameFilters.push_back(OutputNameFilter(filter._type, filter._path));
        }
    }

    void CommandNode::updateMandatoryOutputs(std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs) {
        OutputNodes newMandatoryOutputs;
        for (auto const& output : outputs) newMandatoryOutputs.insert({ output->name(), output });
        std::vector<std::shared_ptr<GeneratedFileNode>> toRemove;
        std::vector<std::shared_ptr<GeneratedFileNode>> toAdd;
        for (auto const& pair : newMandatoryOutputs) {
            auto& newOutput = pair.second;
            if (!_mandatoryOutputs.contains(newOutput->name())) {
                toAdd.push_back(newOutput);
            }
        }
        for (auto const& pair : _mandatoryOutputs) {
            auto& oldOutput = pair.second;
            if (!newMandatoryOutputs.contains(oldOutput->name())) {
                toRemove.push_back(oldOutput);
            }
        }
        for (auto& n : toRemove) {
            n->deleteFile(false, true);
            n->removeObserver(this);
            DirectoryNode::removeGeneratedFile(n);
        }
        for (auto& n : toAdd) n->addObserver(this);
        if (!toRemove.empty() || !toAdd.empty()) _mandatoryOutputs = newMandatoryOutputs;
    }

    void CommandNode::clearOptionalOutputs() {
        for (auto const &pair : _detectedOptionalOutputs) {
            auto& output = pair.second;
            output->deleteFile(false, true);
            output->removeObserver(this);
            context()->nodes().remove(output);
        }
        _detectedOptionalOutputs.clear();
    }

    void CommandNode::mandatoryOutputs(std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs) {
        std::filesystem::path wdir = workingDirectory()->absolutePath();
        std::vector<OutputFilter> filters;
        for (auto const& output : outputs) {
            OutputFilter filter(OutputFilter::Type::Output, output->name());
            filters.push_back(filter);
        }
        outputFilters(filters, outputs);
    }

    CommandNode::OutputNodes const& CommandNode::mandatoryOutputs() const {
        return _mandatoryOutputs;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>> CommandNode::mandatoryOutputsVec() const {
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs;
        for (auto const& pair : _mandatoryOutputs) outputs.push_back(pair.second);
        return outputs;
    }

    CommandNode::OutputNodes const& CommandNode::detectedOptionalOutputs() const {
        if (state() != Node::State::Ok ) throw std::exception("illegal state");
        return _detectedOptionalOutputs;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>> CommandNode::detectedOutputs() const {
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs;
        for (auto const& pair : _mandatoryOutputs) outputs.push_back(pair.second);
        for (auto const& pair : _detectedOptionalOutputs) outputs.push_back(pair.second);
        return outputs;
    }

    CommandNode::InputNodes const& CommandNode::detectedInputs() const {
        if (state() != Node::State::Ok) throw std::exception("illegal state");
        return _detectedInputs;
    }

    void CommandNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        for (auto const& pair : _mandatoryOutputs) outputs.push_back(pair.second);
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

    XXH64_hash_t CommandNode::executionHash() const {
        return _executionHash;
    }

    XXH64_hash_t CommandNode::computeExecutionHash(std::vector<OutputNameFilter> const& filters) const {
        XXH64_state_t* state = XXH64_createState();
        XXH64_reset(state, 0);

        auto wdir = _workingDir.lock();
        if (wdir != nullptr) {
            std::string wdname = wdir->name().generic_string();
            XXH64_update(state, wdname.data(), wdname.length());
        }
        XXH64_update(state, _script.data(), _script.length());
        for (auto const& node : _cmdInputs) {
            std::string nname = node->name().string();
            XXH64_update(state, nname.data(), nname.length());
            auto grpNode = dynamic_pointer_cast<GroupNode>(node);
            if (grpNode != nullptr) {
                XXH64_hash_t grpHash = grpNode->hash();
                XXH64_update(state, &grpHash, sizeof(XXH64_hash_t));
            }
        }
        for (auto const& node : _orderOnlyInputs) {
            std::string nname = node->name().string();
            XXH64_update(state, nname.data(), nname.length());
            auto grpNode = dynamic_pointer_cast<GroupNode>(node);
            if (grpNode != nullptr) {
                XXH64_hash_t grpHash = grpNode->hash();
                XXH64_update(state, &grpHash, sizeof(grpHash));
            }
        }
        for (auto const& filter : filters) {
            XXH64_update(state, &(filter._type), sizeof(filter._type));
            std::string fpath = filter._symPath.generic_string();
            XXH64_update(state, fpath.data(), fpath.length());
        }
        auto entireFile = FileAspect::entireFileAspect().name();
        for (auto const& pair : _mandatoryOutputs) {
            XXH64_hash_t hef = pair.second->hashOf(entireFile);
            XXH64_update(state, &hef, sizeof(hef));
        }
        for (auto const& pair : _detectedOptionalOutputs) {
            XXH64_hash_t hef = pair.second->hashOf(entireFile);
            XXH64_update(state, &hef, sizeof(hef));
        }
        FileAspectSet inputAspects = context()->findFileAspectSet(_inputAspectsName);
        for (auto const& pair : _detectedInputs) {
            auto const& node = pair.second;
            FileAspect const& inputAspect = inputAspects.findApplicableAspect(node->name());
            XXH64_hash_t hdi = node->hashOf(inputAspect.name());
            XXH64_update(state, &hdi, sizeof(hdi));
        }

        XXH64_hash_t hash = XXH64_digest(state);
        XXH64_freeState(state);
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
        bool changed = !result._removedInputPaths.empty() || !result._addedInputNodes.empty();
        if (changed) modified(true);
    }

    // newOptionals are newly created nodes that were not added to context.
    // allOptionals are all found optional outputs, i.e. includes newOptionals.
    // See findOutputNodes.
    void CommandNode::setDetectedOptionalOutputs(
        std::vector<std::shared_ptr<GeneratedFileNode>> const& allOptionals,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& newOptionals
    ) {
        std::vector<std::shared_ptr<GeneratedFileNode>> toRemove;
        auto end = allOptionals.end();
        for (auto const& pair : _detectedOptionalOutputs) {
            if (end == std::find(allOptionals.begin(), end, pair.second)) {
                toRemove.push_back(pair.second);
            }
        }
        for (auto const &n : toRemove) {
            n->removeObserver(this);
            _detectedOptionalOutputs.erase(n->name());
            DirectoryNode::removeGeneratedFile(n);
            context()->nodes().remove(n);
        }
        for (auto& n : newOptionals) {
            context()->nodes().add(n);
            _detectedOptionalOutputs.insert({ n->name(), n });
            n->addObserver(this);
        }
        for (auto const& output : allOptionals) {
            DirectoryNode::addGeneratedFile(output);
        }
        bool changed = !toRemove.empty() || !newOptionals.empty();
        if (changed) modified(true);
    }

    CommandNode::OutputFilter::Type CommandNode::findFilterType(
        std::filesystem::path const& symPath,
        std::vector<OutputNameFilter> const& filters
    ) const {
        auto type = OutputFilter::Type::None;
        for (auto const& filter : filters) {
            if (filter._glob.matches(symPath)) {
                switch (filter._type) {
                    case OutputFilter::Type::Output:
                    case OutputFilter::Type::ExtraOutput: {
                        type = OutputFilter::Type::Output;
                        break;
                    }
                    case OutputFilter::Type::Optional: {
                        if (
                            type == OutputFilter::Type::None
                            || type == OutputFilter::Type::Ignore
                        ) {
                            type = OutputFilter::Type::Optional;
                        }
                        break;
                    }
                    case OutputFilter::Type::Ignore: {
                        if (
                            type == OutputFilter::Type::None
                            || type == OutputFilter::Type::Optional
                        ) {
                            type = OutputFilter::Type::Ignore;
                        }
                        break;
                    }
                    default: throw std::exception("illegal filtertype");
                }
            }
        }
        return type;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>> CommandNode::filterOutputs(
        OutputNodes const& outputs,
        CommandNode::OutputFilter::Type filterType
    ) {
        std::vector<std::shared_ptr<GeneratedFileNode>> filtered;
        for (auto const& pair : outputs) {
            auto& node = pair.second;
            if (findFilterType(node->name(), outputNameFilters()) == filterType) {
                filtered.push_back(node);
            }
        }
        return filtered;
    }

    bool CommandNode::findOutputNodes(
        std::map<std::filesystem::path, OutputFilter::Type> const& outputSymPaths,
        std::vector<std::shared_ptr<GeneratedFileNode>>& optionalOutputNodes,
        std::vector<std::shared_ptr<GeneratedFileNode>>& newOptionalOutputNodes,
        MemoryLogBook& logBook
    ) {
        bool valid = true;
        std::vector<std::shared_ptr<GeneratedFileNode>> mandatoryOutputNodes;
        std::vector<std::filesystem::path> ignoredOutputs;
        std::vector<std::filesystem::path> notDeclaredOutputs;
        for (auto const& pair : outputSymPaths) {
            OutputFilter::Type outputType = pair.second;
            std::filesystem::path const& outputPath = pair.first;
            std::shared_ptr<GeneratedFileNode> outputNode;
            std::shared_ptr<Node> node = context()->nodes().find(outputPath);
            if (node == nullptr) {
                // must be optional or ignore output because mandatory output
                // nodes were created in advance, see updateMandatoryNodes().
                if (outputType == OutputFilter::Type::Output) {
                    throw std::exception("illegal filter type");
                } else if (outputType == OutputFilter::Type::Optional) {
                    auto sharedThis = dynamic_pointer_cast<CommandNode>(shared_from_this());
                    outputNode = std::make_shared<GeneratedFileNode>(context(), outputPath, sharedThis);
                    optionalOutputNodes.push_back(outputNode);
                    newOptionalOutputNodes.push_back(outputNode);
                } else if (outputType == OutputFilter::Type::Ignore) {
                    ignoredOutputs.push_back(outputPath);
                } else {
                    notDeclaredOutputs.push_back(outputPath);
                }
            } else {
                outputNode = dynamic_pointer_cast<GeneratedFileNode>(node);
                if (outputNode == nullptr) {
                    auto srcFileNode = dynamic_pointer_cast<SourceFileNode>(node);
                    if (srcFileNode != nullptr) {
                        valid = false;
                        logWriteAccessedSourceFile(this, srcFileNode.get(), logBook);
                    } else {
                        throw std::exception("unexpected node type");
                    }
                } else if (outputType == OutputFilter::Type::Ignore) {
                    // Must be an output of another node.
                    if (outputNode->producer().get() == this) throw std::exception("unexpected registered node");
                } else if (outputType == OutputFilter::Type::Optional) {
                    optionalOutputNodes.push_back(outputNode);
                } else if (outputType == OutputFilter::Type::Output) {
                    mandatoryOutputNodes.push_back(outputNode);
                } else {
                    notDeclaredOutputs.push_back(outputNode->name());
                }
                if (outputNode != nullptr && outputNode->producer().get() != this) {
                    valid = false;
                    logAlreadyProducedByOtherCommand(this, outputNode.get(), outputType, logBook);
                }
            }
        }
        if (!verifyMandatoryOutputs(mandatoryOutputNodes, logBook)) valid = false;
        for (auto const& outputPath : notDeclaredOutputs) {
            valid = false;
            logNotDeclaredOutput(this, outputPath, logBook);
            deleteNotDeclaredOutput(this, outputPath, logBook);
        }
        for (auto const& outputPath : ignoredOutputs) {
            deleteIgnoredOutput(this, outputPath, logBook);;
        }
        return valid;
    }

    bool CommandNode::verifyMandatoryOutputs(
        std::vector<std::shared_ptr<GeneratedFileNode>> const& outputNodes,
        MemoryLogBook& logBook
    ) {
        bool valid = outputNodes.size() == _mandatoryOutputs.size();
        if (valid) {
            for (auto const& node : outputNodes) {
                valid = _mandatoryOutputs.contains(node->name());
                if (!valid) break;
            }
            if (valid) {
                auto end = outputNodes.end();
                for (auto const& pair : _mandatoryOutputs) {
                    valid = end != std::find(outputNodes.begin(), end, pair.second);
                    if (!valid) break;
                }
            }
        }
        if (!valid) {
            std::set<std::filesystem::path> mandatoryPaths;
            std::set<std::filesystem::path> outputPaths;
            for (auto const& pair : _mandatoryOutputs) mandatoryPaths.insert(pair.second->name());
            for (auto const& node : outputNodes) outputPaths.insert(node->name());
            logUnexpectedOutputs(this, mandatoryPaths, outputPaths, logBook);
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

    void CommandNode::postProcessor(std::shared_ptr<PostProcessor> const& processor) {
        _postProcessor = processor;
    }

    // main thread
    void CommandNode::start(PriorityClass prio) {
        Node::start(prio);
        std::vector<Node*> requisites;
        for (auto const& ip : _inputProducers) {
            auto const& group = dynamic_pointer_cast<GroupNode>(ip);
            if (group != nullptr && group->content().empty()) {
                std::stringstream ss;
                ss 
                    << "Input group " << ip->name() << " at line " << _ruleLineNr
                    << " in file " << _buildFile->name() << " is empty." << std::endl;
                ss << "Please make sure that output files are added to this group." << std::endl;
                LogRecord w(LogRecord::Aspect::Warning, ss.str());
                context()->addToLogBook(w);
            }
            requisites.push_back(ip.get());
        }
        getSourceInputs(requisites);
        for (auto const& pair : _mandatoryOutputs) requisites.push_back(pair.second.get());
        for (auto const& pair : _detectedOptionalOutputs) requisites.push_back(pair.second.get());
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State state) { handleRequisitesCompletion(state); }
        );
        startNodes(requisites, std::move(callback), prio);
    }

    void CommandNode::handleRequisitesCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            Node::notifyCompletion(state);
        } else if (canceling()) {
            Node::notifyCompletion(Node::State::Canceled);
        } else if (_executionHash != computeExecutionHash(outputNameFilters())) {
            context()->statistics().registerSelfExecuted(this);
            for (auto const& pair : _mandatoryOutputs) {
                DirectoryNode::addGeneratedFile(pair.second);
            }
            auto d = Delegate<void>::CreateLambda(
                [this]() { executeScript(); }
            );
            context()->threadPoolQueue().push(std::move(d), PriorityClass::High);
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
            ss << "File " << absPath.string() << " is used as input or output by script: " << std::endl;
            ss << _script << std::endl;
            if (_buildFile != nullptr) {
                ss << "This script is defined in buildfile " << _buildFile->name() << " by the rule at line " << _ruleLineNr << std::endl;
            }
            ss << "This file is not part of a known file repository and will not be tracked for changes." << std::endl;
            ss << "To get rid of this warning you must declare the file repository that contains the file." << std::endl;
            ss << "Set the repository type to: " << std::endl;
            ss << "    Build if it is a yam repository that must be build." << std::endl;
            ss << "    Track if yam must only track dependencies on files in this repository." << std::endl;
            ss << "    Ignore if yam must not track dependencies on files in this repository." << std::endl;
            LogRecord warning(LogRecord::Warning, ss.str());
            logBook.add(warning);
            return "";
        } else if (repo->repoType() != FileRepositoryNode::Ignore) {
            return repo->symbolicPathOf(absPath);
        } else {
            return "";
        }
    }

    std::set<std::filesystem::path> CommandNode::convertToSymbolicPaths(
        std::set<std::filesystem::path> const& absPaths,
        MemoryLogBook& logBook
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
        result->_log.aspects(context()->logBook()->aspects());
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

                std::set<std::filesystem::path> outputPaths = convertToSymbolicPaths(scriptResult.writtenFiles, result->_log);
                for (auto const& path : outputPaths) {
                    result->_outputPaths.insert({ path,findFilterType(path, outputNameFilters())});
                }
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
            std::vector<std::shared_ptr<GeneratedFileNode>> optionalOutputNodes;
            std::vector<std::shared_ptr<GeneratedFileNode>> newOptionalOutputNodes;
            bool validOutputs = findOutputNodes(result._outputPaths, optionalOutputNodes, newOptionalOutputNodes, result._log);
            if (!validOutputs) {
                result._newState = Node::State::Failed;
            } else {
                std::vector< std::shared_ptr<Node>> outputsAndNewInputs;
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
                    setDetectedOptionalOutputs(optionalOutputNodes, newOptionalOutputNodes);
                    // output nodes have been updated by command script, hence their
                    // hashes need to be re-computed.
                    for (auto& pair : _mandatoryOutputs) {
                        auto& n = pair.second;
                        // temporarily stop observing n to avoid Dirty state to
                        // propagate to this command (see handleDirtyOf()).
                        n->removeObserver(this);
                        n->setState(Node::State::Dirty);
                        n->addObserver(this);
                        outputsAndNewInputs.push_back(n);
                    }
                    for (auto const &pair : _detectedOptionalOutputs) {
                        // temporarily stop observing n to avoid Dirty state to
                        // propagate to this command (see handleDirtyOf()).
                        auto const& n = pair.second;
                        n->removeObserver(this);
                        n->setState(Node::State::Dirty);
                        n->addObserver(this);
                        outputsAndNewInputs.push_back(n);
                    }
                    std::vector<Node*> rawNodes;
                    for (auto const& n : outputsAndNewInputs) rawNodes.push_back(n.get());
                    auto callback = Delegate<void, Node::State>::CreateLambda(
                        [this, sresult](Node::State state) { 
                            handleOutputAndNewInputFilesCompletion(state, sresult); }
                    );
                    startNodes(rawNodes, std::move(callback), PriorityClass::VeryHigh);
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
            _executionHash = computeExecutionHash(outputNameFilters());
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
            clearDetectedInputs();
            for (auto const &pair : _mandatoryOutputs) pair.second->deleteFile();
            for (auto const &pair : _detectedOptionalOutputs) pair.second->deleteFile();
            _executionHash = rand();
        }
        modified(true);
        notifyCompletion(sresult->_newState);
    }

    std::string CommandNode::compileScript(ILogBook& logBook) {
        std::filesystem::path wdir;
        auto workingDir = _workingDir.lock();
        if (workingDir == nullptr) return _script;
        if (!PercentageFlagsCompiler::containsFlags(_script)) return _script;

        BuildFile::Script bfScript;
        bfScript.script = _script;
        bfScript.line = _ruleLineNr;
        std::filesystem::path buildFile = _buildFile == nullptr ? "" : _buildFile->name();
        std::vector<std::shared_ptr<Node>> cmdInputs = getFileNodes(_cmdInputs);
        std::vector<std::shared_ptr<Node>> orderOnlyInputs = getFileNodes(_orderOnlyInputs);
        std::vector<std::shared_ptr<GeneratedFileNode>> nonExtraOutputs = filterOutputs(_mandatoryOutputs, OutputFilter::Type::Output);
        try {
            PercentageFlagsCompiler compiler(
                buildFile,
                bfScript,
                workingDir,
                cmdInputs,
                orderOnlyInputs,
                nonExtraOutputs);
            return compiler.result();
        } catch (std::runtime_error e) {
            LogRecord error(LogRecord::Error, e.what());
            logBook.add(error);
            return "";
        }
    }

    // threadpool
    MonitoredProcessResult CommandNode::executeMonitoredScript(ILogBook& logBook) {
        for (auto const &pair : _mandatoryOutputs) pair.second->deleteFile();
        for (auto const &pair : _detectedOptionalOutputs) pair.second->deleteFile();

        std::string script = compileScript(logBook);
        if (script.empty()) return MonitoredProcessResult{ 1 };

        std::filesystem::path tmpDir = FileSystem::createUniqueDirectory("cmd_");
        auto scriptFilePath = std::filesystem::path(tmpDir / "cmdscript.cmd");
        std::ofstream scriptFile(scriptFilePath.string());
        scriptFile << "@echo off" << std::endl;
        scriptFile << script << std::endl;
        scriptFile.close();

        std::map<std::string, std::string> env;
        env["TMP"] = tmpDir.string();
        env["TEMP"] = tmpDir.string();

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
        } else  {
            bool stop = true;
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

    void CommandNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(_inputAspectsName);
        streamer->streamVector(_cmdInputs);
        streamer->streamVector(_orderOnlyInputs);
        std::shared_ptr<DirectoryNode> wdir;
        if (streamer->writing()) wdir = _workingDir.lock();
        streamer->stream(wdir);
        if (streamer->reading()) _workingDir = wdir;
        streamer->stream(_script);
        OutputFilter::streamVector(streamer, _outputFilters);
        NodeMapStreamer::stream(streamer, _mandatoryOutputs);
        NodeMapStreamer::stream(streamer, _detectedOptionalOutputs);
        NodeMapStreamer::stream(streamer, _detectedInputs);
        streamer->stream(_executionHash);
    }

    void CommandNode::prepareDeserialize() {
        Node::prepareDeserialize();
        for (auto const& i : _inputProducers) i->removeObserver(this);
        for (auto const& p : _mandatoryOutputs) p.second->removeObserver(this);
        for (auto const& p : _detectedOptionalOutputs) p.second->removeObserver(this);
        for (auto const& p : _detectedInputs) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(p.second);
            if (genFile == nullptr) p.second->removeObserver(this);
        }
        _cmdInputs.clear();
        _orderOnlyInputs.clear();
        _outputFilters.clear();
        _inputProducers.clear();
        _outputNameFilters.clear();
        _mandatoryOutputs.clear();
        _detectedOptionalOutputs.clear();
        _detectedInputs.clear();
    }

    bool CommandNode::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!Node::restore(context, restored)) return false;
        for (auto const& node : _cmdInputs) {
            node->restore(context, restored);
        }
        for (auto const& genFileNode : _orderOnlyInputs) {
            genFileNode->restore(context, restored);
        }
        auto wdir = _workingDir.lock();
        if (wdir != nullptr) wdir->restore(context, restored);

        updateInputProducers();

        // Cannot update outputname filters in restore because it
        // relies on workingDir to have an initalized _parent field.
        // This however is only guaranteed after all restores have
        // completed. Therefore updateOutputNameFilters() is called
        // lazily by outputNameFilters().
        // Another solution is to also stream the references to the
        // parent directory. 
        //updateOutputNameFilters();

        NodeMapStreamer::restore(_mandatoryOutputs);
        for (auto const& pair : _mandatoryOutputs) {
            pair.second->restore(context, restored);
            pair.second->addObserver(this);
        }
        NodeMapStreamer::restore(_detectedOptionalOutputs);
        for (auto const& pair : _detectedOptionalOutputs) {
            pair.second->restore(context, restored);
            pair.second->addObserver(this);
        }
        NodeMapStreamer::restore(_detectedInputs);
        for (auto const& pair : _detectedInputs) {
            pair.second->restore(context, restored);
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(pair.second);
            if (genFile == nullptr) pair.second->addObserver(this);
        }
        return true;
    }
}
