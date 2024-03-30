#include "ForEachNode.h"
#include "CommandNode.h"
#include "DirectoryNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "FileRepositoryNode.h"
#include "GroupNode.h"
#include "ExecutionContext.h"
#include "PercentageFlagsCompiler.h"
#include "NodeMapStreamer.h"
#include "BuildFileCompiler.h"
#include "IStreamer.h"
#include "computeMapsDifference.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <utility>

namespace
{
    using namespace YAM;

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

    void addHashes(
        std::vector<std::shared_ptr<Node>> const& inputs,
        std::vector<XXH64_hash_t>& hashes
    ) {
        for (auto const& node : inputs) {
            hashes.push_back(XXH64_string(node->name().string()));
            auto grpNode = dynamic_pointer_cast<GroupNode>(node);
            if (grpNode != nullptr) hashes.push_back(grpNode->hash());
        }
    }
}

namespace YAM
{
    ForEachNode::ForEachNode()
        : Node()
        , _buildFile(nullptr) {}

    ForEachNode::ForEachNode(
        ExecutionContext* context,
        std::filesystem::path const& name)
        : Node(context, name)
        , _buildFile(nullptr)
        , _executionHash(rand())
    {}

    ForEachNode::~ForEachNode() {
        destroy(false);
    }

    void ForEachNode::destroy(bool removeFromContext) {
        _cmdInputs.clear();
        _orderOnlyInputs.clear();
        for (auto const& group : _inputGroups) {
            group->removeObserver(this);
        }
        _inputGroups.clear();
        _script.clear();
        if (removeFromContext) {
            for (auto const& cmd : _commands) {
                cmd->cmdInputs({});
                cmd->orderOnlyInputs({});
                cmd->script("");
                cmd->workingDirectory(nullptr);
                cmd->outputFilters({}, {});
                cmd->modified(true);
                context()->nodes().remove(cmd);
            }
        }
        _commands.clear();
    }

    // ForEachNode is removed from context: clear members variables
    void ForEachNode::cleanup() {
        _cmdInputs.clear();
        _orderOnlyInputs.clear();
        for (auto const& group : _inputGroups) {
            group->removeObserver(this);
        }
        _inputGroups.clear();
        _script.clear();
    }

    void ForEachNode::cmdInputs(std::vector<std::shared_ptr<Node>> const& newInputs) {
        if (_cmdInputs != newInputs) {
            _cmdInputs = newInputs;
            updateInputGroups();
            modified(true);
            setState(State::Dirty);
        }
    }
    std::vector<std::shared_ptr<Node>> const& ForEachNode::cmdInputs() const {
        return _cmdInputs;
    }

    void ForEachNode::orderOnlyInputs(std::vector<std::shared_ptr<Node>> const& newInputs) {
        if (_orderOnlyInputs != newInputs) {
            _orderOnlyInputs = newInputs;
            modified(true);
            setState(State::Dirty);
        }
    }
    std::vector<std::shared_ptr<Node>> const& ForEachNode::orderOnlyInputs() const {
        return _orderOnlyInputs;
    }

    std::vector<std::shared_ptr<GroupNode>> ForEachNode::inputGroups() const {
        std::vector<std::shared_ptr<GroupNode>> groups;
        getGroups(_cmdInputs, groups);
        getGroups(_orderOnlyInputs, groups);
        return groups;
    }

    void ForEachNode::updateInputGroups() {
        for (auto group : _inputGroups) group->removeObserver(this);
        _inputGroups.clear();
        getGroups(_cmdInputs, _inputGroups);
        for (auto const& group : _inputGroups) group->addObserver(this);
    }

    void ForEachNode::script(std::string const& newScript) {
        if (newScript != _script) {
            _script = newScript;
            modified(true);
            setState(State::Dirty);
        }
    }
    std::string const& ForEachNode::script() const {
        return _script;
    }

    void ForEachNode::workingDirectory(std::shared_ptr<DirectoryNode> const& dir) {
        if (_workingDir.lock() != dir) {
            _workingDir = dir;
            modified(true);
            setState(State::Dirty);
        }
    }
    std::shared_ptr<DirectoryNode> ForEachNode::workingDirectory() const {
        auto wdir = _workingDir.lock();
        if (wdir == nullptr) {
            auto repo = repository();
            if (repo != nullptr) wdir = repo->directoryNode();
        }
        return wdir;
    }

    void ForEachNode::outputs(BuildFile::Outputs const& newOutputs) {
        if (_outputs != newOutputs) {
            _outputs = newOutputs;
            modified(true);
            setState(State::Dirty);
        }
    }

    BuildFile::Outputs const& ForEachNode::outputs() const {
        return _outputs;
    }

    void ForEachNode::buildFile(SourceFileNode* buildFile) {
        _buildFile = buildFile;
    }
    void ForEachNode::ruleLineNr(std::size_t ruleLineNr) {
        _ruleLineNr = ruleLineNr;
    }
    SourceFileNode const* ForEachNode::buildFile() const {
        return _buildFile;
    }
    std::size_t ForEachNode::ruleLineNr() const {
        return _ruleLineNr;
    }

    XXH64_hash_t ForEachNode::executionHash() const {
        return _executionHash;
    }

    XXH64_hash_t ForEachNode::computeExecutionHash() const {
        std::vector<XXH64_hash_t> hashes;
        auto wdir = _workingDir.lock();
        if (wdir != nullptr) hashes.push_back(XXH64_string(wdir->name().string()));
        hashes.push_back(XXH64_string(_script));
        addHashes(_cmdInputs, hashes);
        addHashes(_orderOnlyInputs, hashes);
        _outputs.addHashes(hashes);
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    // main thread
    void ForEachNode::start(PriorityClass prio) {
        Node::start(prio);
        std::vector<Node*> requisites;
        for (auto const& group : _inputGroups) {
            if (group->content().empty()) {
                std::stringstream ss;
                ss
                    << "Input group " << group->name() << " at line " << _ruleLineNr
                    << " in file " << _buildFile->name() << " is empty." << std::endl;
                ss << "Please make sure that output files are added to this group." << std::endl;
                LogRecord w(LogRecord::Aspect::Warning, ss.str());
                context()->addToLogBook(w);
            }
            requisites.push_back(group.get());
        }
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State state) { handleRequisitesCompletion(state); }
        );
        startNodes(requisites, std::move(callback), prio);
    }

    void ForEachNode::handleRequisitesCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            Node::notifyCompletion(state);
        } else if (canceling()) {
            Node::notifyCompletion(Node::State::Canceled);
        } else {
            Node::State newState = Node::State::Ok;
            XXH64_hash_t newHash = computeExecutionHash();
            if (newHash != _executionHash) {
                context()->statistics().registerSelfExecuted(this);
                bool compiled = compileCommands();
                if (compiled) {
                    _executionHash = newHash;
                } else {
                    newState = Node::State::Failed;
                }
                modified(true);
            }
            if (newState != Node::State::Ok) {
                Node::notifyCompletion(state);
            } else {
                auto callback = Delegate<void, Node::State>::CreateLambda(
                    [this](Node::State state) {
                    handleCommandsCompletion(state); }
                );
                startNodes(_commands, std::move(callback), PriorityClass::VeryHigh);
            }
        } 
    }

    bool ForEachNode::compileCommands() {
        std::vector<std::shared_ptr<FileNode>> files = cmdInputFiles();
        BuildFile::File rulesFile;
        for (auto const& file : files) {
            rulesFile.variablesAndRules.push_back(createRule(file));
        }

        // TODO: remove code duplication with BuildFileCompilerNode.
        //
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> oldCommands;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> oldMandatoryOutputs;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> emptyOutputGroups;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> allowedInputs;

        for (auto const& cmd : _commands) {
            oldCommands.insert({ cmd->name(), cmd });
            for (auto const& pair : cmd->mandatoryOutputs()) oldMandatoryOutputs.insert(pair);
        }
        for (auto const& file : files) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(file);
            if (genFile != nullptr) allowedInputs.insert({ genFile->name(), genFile });
        }

        bool compiled = true;
        try {
            BuildFileCompiler compiler(
                context(),
                _workingDir.lock(),
                rulesFile,
                oldCommands,
                oldMandatoryOutputs,
                emptyOutputGroups,
                allowedInputs);
            std::map<std::filesystem::path, std::shared_ptr<CommandNode>> newCommands;
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> newMandatoryOutputs;
            updateMap(context(), this, newCommands, compiler.commands());
            updateMap(context(), this, newMandatoryOutputs, compiler.mandatoryOutputs());
            _commands.clear();
            for (auto const& pair : newCommands) {
                auto const& cmd = pair.second;
                cmd->orderOnlyInputs(_orderOnlyInputs);
                _commands.push_back(cmd);
            }
        } catch (std::runtime_error e) {
            compiled = false;
            LogRecord error(LogRecord::Error, e.what());
            context()->logBook()->add(error);
        }

        return compiled;
    }

    std::shared_ptr<BuildFile::Rule> ForEachNode::createRule(std::shared_ptr<FileNode> const& inputFile) {
        auto rule = std::make_shared<BuildFile::Rule>();
        rule->line = _ruleLineNr;
        rule->forEach = false;

        auto const& repo = inputFile->repository();
        auto inputPath = repo->relativePathOf(inputFile->name());
        BuildFile::Input input;
        input.line = _ruleLineNr;
        input.exclude = false;
        input.path = inputPath;
        input.pathType = BuildFile::PathType::Path;
        rule->cmdInputs.line = _ruleLineNr;
        rule->cmdInputs.inputs.push_back(input);

        rule->script.line = _ruleLineNr;
        rule->script.script = _script;

        rule->outputs.line = _ruleLineNr;
        auto& outputs = rule->outputs.outputs;
        outputs.insert(outputs.end(), _outputs.outputs.begin(), _outputs.outputs.end());

        return rule;
    }

    std::vector<std::shared_ptr<FileNode>> ForEachNode::cmdInputFiles() const {
        std::vector<std::shared_ptr<FileNode>> files;
        for (auto const& n : _cmdInputs) {
            auto const& group = dynamic_pointer_cast<GroupNode>(n);
            if (group == nullptr) {
                auto const& file = dynamic_pointer_cast<FileNode>(n);
                if (file != nullptr) files.push_back(file);
            } else {
                std::set<std::shared_ptr<FileNode>, Node::CompareName> filesInGroup;
                filesInGroup = group->files();
                files.insert(files.end(), filesInGroup.begin(), filesInGroup.end());
            }
        }
        return files;
    }

    void ForEachNode::handleCommandsCompletion(Node::State newState) {
        modified(true);
        notifyCompletion(newState);
    }
    void ForEachNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t ForEachNode::typeId() const {
        return streamableTypeId;
    }

    void ForEachNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->streamVector(_cmdInputs);
        streamer->streamVector(_orderOnlyInputs);
        std::shared_ptr<DirectoryNode> wdir;
        if (streamer->writing()) wdir = _workingDir.lock();
        streamer->stream(wdir);
        if (streamer->reading()) _workingDir = wdir;
        streamer->stream(_script);
        _outputs.stream(streamer);
        streamer->stream(_executionHash);
    }

    void ForEachNode::prepareDeserialize() {
        Node::prepareDeserialize();
        for (auto const& g : _inputGroups) g->removeObserver(this);
        _cmdInputs.clear();
        _orderOnlyInputs.clear();
        _outputs.outputs.clear();
        _inputGroups.clear();
    }

    bool ForEachNode::restore(void* context, std::unordered_set<IPersistable const*>& restored) {
        if (!Node::restore(context, restored)) return false;
        for (auto const& node : _cmdInputs) {
            node->restore(context, restored);
        }
        for (auto const& genFileNode : _orderOnlyInputs) {
            genFileNode->restore(context, restored);
        }
        auto wdir = _workingDir.lock();
        if (wdir != nullptr) wdir->restore(context, restored);

        updateInputGroups();
        return true;
    }
}
