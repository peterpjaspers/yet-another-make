#include "BuildFileCompilerNode.h"
#include "BuildFileParserNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "DirectoryNode.h"
#include "GeneratedFileNode.h"
#include "FileRepositoryNode.h"
#include "CommandNode.h"
#include "GlobNode.h"
#include "GroupNode.h"
#include "ExecutionContext.h"
#include "FileSystem.h"
#include "FileAspect.h"
#include "BuildFileParser.h"
#include "BuildFileDependenciesCompiler.h"
#include "BuildFileCompiler.h"
#include "IStreamer.h"
#include "NodeMapStreamer.h"
#include "computeMapsDifference.h"

#include <vector>
#include <unordered_set>


namespace
{
    using namespace YAM;

    uint32_t streamableTypeId = 0;

    void addNode(std::shared_ptr<BuildFileCompilerNode> compiler, StateObserver* observer) {
        // owned and added to context by DirectoryNode
        compiler->addObserver(observer);
    }
    void addNode(std::shared_ptr<GlobNode> glob, StateObserver* observer) {
        // A glob node can be shared by multiple compilers.
        glob->context()->nodes().addIfAbsent(glob);
        glob->addObserver(observer);
    }

    void addNode(std::shared_ptr<CommandNode> node, StateObserver* observer) {
        node->context()->nodes().add(node);
    }
    void addNode(std::shared_ptr<GeneratedFileNode> node, StateObserver* observer) {
        node->context()->nodes().add(node);
    }
    void addNode(std::shared_ptr<GroupNode> group, StateObserver* observer) {
        // A group node can be shared by multiple compilers and can already
        // have been added to the context by another compiler.
        group->context()->nodes().addIfAbsent(group);
    }

    void removeNode(std::shared_ptr<BuildFileCompilerNode> compiler, StateObserver* observer) {
        // owned by DirectoryNode
        compiler->removeObserver(observer);
    }
    void removeNode(std::shared_ptr<GlobNode> glob, StateObserver* observer) {
        // A glob node can be shared by multiple compilers.
        glob->removeObserver(observer);
        if (glob->observers().empty()) {
            glob->setState(Node::State::Deleted);
            glob->modified(true);
            glob->context()->nodes().remove(glob);
        }
    }
    void removeNode(std::shared_ptr<CommandNode> cmd, StateObserver* observer) {
        static std::vector<std::shared_ptr<GeneratedFileNode>> emptyOutputs;
        static std::vector<std::shared_ptr<Node>> emptyCmdInputs;
        static std::vector<std::shared_ptr<Node>> emptyOrderOnlyInputs;
        ExecutionContext* context = cmd->context();
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs = cmd->outputs();
        cmd->cmdInputs(emptyCmdInputs);
        cmd->orderOnlyInputs(emptyOrderOnlyInputs);
        cmd->script("");
        cmd->workingDirectory(nullptr);
        cmd->outputs(emptyOutputs);
        cmd->ignoreOutputs({});
        cmd->setState(Node::State::Deleted);
        cmd->modified(true);
        cmd->context()->nodes().remove(cmd);
    }
    void removeNode(std::shared_ptr<GeneratedFileNode> node, StateObserver* observer) {
        ExecutionContext* context = node->context();
        node->setState(Node::State::Deleted);
        node->modified(true);
        node->context()->nodes().remove(node);
    }
    void removeNode(std::shared_ptr<GroupNode> group, StateObserver* observer) {
        if (group->content().empty() && group->observers().empty()) {
            group->setState(Node::State::Deleted);
            group->modified(true);
            group->context()->nodes().remove(group);
        }
    }

    template <class TNode>
    void updateMap(
        ExecutionContext* context,
        StateObserver* observer,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& toUpdate,
        std::map<std::filesystem::path, std::shared_ptr<TNode>> const& newSet
    ) {
        std::map<std::filesystem::path, std::shared_ptr<TNode>> kept;
        std::map<std::filesystem::path, std::shared_ptr<TNode>> added;
        std::map<std::filesystem::path, std::shared_ptr<TNode>> removed;
        computeMapsDifference(newSet, toUpdate, kept, added, removed);
        for (auto const& pair : added) addNode(pair.second, observer);
        for (auto const& pair : removed) removeNode(pair.second, observer);
        toUpdate = newSet;
    }

    void updateLineNrs(
        std::vector<std::size_t>& lineNrs, 
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const& cmds,
        std::map<std::shared_ptr<CommandNode>, std::size_t> const& lineNrsMap,
        std::shared_ptr<SourceFileNode> buildFile
    ) {
        lineNrs.clear();
        for (auto const& pair : cmds) {
            auto cmd = pair.second;
            auto it = lineNrsMap.find(cmd);
            std::size_t lineNr = it->second;
            lineNrs.push_back(lineNr);
            cmd->buildFile(buildFile.get());
            cmd->ruleLineNr(lineNr);
        }
    }

    BuildFileParserNode const* findParser(
        std::vector<BuildFileParserNode const*> const& parsers,
        SourceFileNode const* buildFile
     ) {
        for (auto parser : parsers) {
            if (parser->buildFile().get() == buildFile) return parser;
        }
        return nullptr;
    }

    bool containsParser(
        std::vector<BuildFileParserNode const*> const& parsers, 
        SourceFileNode const* buildFile
    ) {
        return nullptr != findParser(parsers, buildFile);
    }


    void findNotUsedParsers(
        std::vector<BuildFileParserNode const*> const& allParsers,
        std::unordered_set<BuildFileParserNode const*> const& usedParsers,
        std::vector<BuildFileParserNode const*>& notUsedParsers
    ) {
        for (auto p : allParsers) {
            if (usedParsers.find(p) == usedParsers.end()) {
                notUsedParsers.push_back(p);
            }
        }
    }

    std::map<std::filesystem::path, std::shared_ptr<BuildFileCompilerNode>> findCompilerDependencies(
        std::shared_ptr<BuildFileParserNode> const& parser
    ) {
        std::map<std::filesystem::path, std::shared_ptr<BuildFileCompilerNode>> depCompilers;
        for (auto depParser : parser->dependencies()) {
            auto dir = depParser->buildFileDirectory();
            auto depCompiler = dir->buildFileCompilerNode();
            depCompilers.insert({ depCompiler->name(), depCompiler });
        }
        return depCompilers;
    }
}

namespace YAM
{

    BuildFileCompilerNode::BuildFileCompilerNode() {}

    BuildFileCompilerNode::BuildFileCompilerNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
        , _executionHash(rand())
    {
    }

    BuildFileCompilerNode::~BuildFileCompilerNode() {
        if (_buildFileParser != nullptr)  _buildFileParser->removeObserver(this);
        for (auto const& pair : _depCompilers) pair.second->removeObserver(this);
        for (auto const& pair : _depGlobs) pair.second->removeObserver(this);
        _buildFileParser = nullptr;
    }

    void BuildFileCompilerNode::buildFileParser(std::shared_ptr<BuildFileParserNode> const& newParser) {
        static std::map<std::filesystem::path, std::shared_ptr<CommandNode>> emptyCmds;
        static std::map<std::filesystem::path, std::shared_ptr<GroupNode>> emptyGroups;
        static std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> emptyOutputs;
        static std::map<std::filesystem::path, std::shared_ptr<BuildFileCompilerNode>> emptyCompilers;
        static std::map<std::filesystem::path, std::shared_ptr<GlobNode>> emptyGlobs;
        if (_buildFileParser != newParser) {
            if (_buildFileParser != nullptr) {
                _buildFileParser->removeObserver(this);
                for (auto const& pair : _outputGroups) cleanOutputGroup(pair.second.get());
                updateMap(context(), this, _commands, emptyCmds);
                updateMap(context(), this, _outputGroups, emptyGroups);
                updateMap(context(), this, _outputs, emptyOutputs);
                updateMap(context(), this, _depCompilers, emptyCompilers);
                updateMap(context(), this, _depGlobs, emptyGlobs);
            }
            _buildFileParser = newParser;
            if (_buildFileParser != nullptr) {
                _buildFileParser->addObserver(this);
            }
            _executionHash = rand();
            modified(true);
            setState(Node::State::Dirty);
        }
    }

    std::shared_ptr<BuildFileParserNode> BuildFileCompilerNode::buildFileParser() const {
        return _buildFileParser;
    }

    XXH64_hash_t BuildFileCompilerNode::computeExecutionHash() const {
        std::vector<XXH64_hash_t> hashes;
        hashes.push_back(_buildFileParser->executionHash());
        for (auto pair : _depCompilers) hashes.push_back(pair.second->_executionHash);
        for (auto pair : _depGlobs) hashes.push_back(pair.second->executionHash());
        hashes.push_back(repository()->hash());
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    void BuildFileCompilerNode::start() {
        Node::start();
        if (_buildFileParser == nullptr) {
            postCompletion(Node::State::Ok);
        } else {
            if (!updateBuildFileDependencies()) {
                postCompletion(Node::State::Failed);
            } else {
                std::vector<std::shared_ptr<Node>> requisites;
                getInputs(requisites);
                auto callback = Delegate<void, Node::State>::CreateLambda(
                    [this](Node::State state) { handleRequisitesCompletion(state); });
                startNodes(requisites, std::move(callback));
            }
        }
    }

    bool BuildFileCompilerNode::updateBuildFileDependencies() {
        try {
            BuildFileDependenciesCompiler compiler(
                context(),
                _buildFileParser->buildFileDirectory(),
                _buildFileParser->parseTree(),
                BuildFileDependenciesCompiler::Mode::InputGlobs);
            updateMap(context(), this, _depGlobs, compiler.globs());
            auto depCompilers = findCompilerDependencies(_buildFileParser);
            updateMap(context(), this, _depCompilers, depCompilers);
            return true;
        } catch (std::runtime_error e) {
            LogRecord error(LogRecord::Error, e.what());
            context()->logBook()->add(error);
            return false;
        }
    }

    void BuildFileCompilerNode::handleRequisitesCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            notifyProcessingCompletion(state);
        } else if (canceling()) {
            notifyProcessingCompletion(Node::State::Canceled);
        } else if (_executionHash == computeExecutionHash()) {
            notifyProcessingCompletion(Node::State::Ok);
        } else {
            context()->statistics().registerSelfExecuted(this);
            compileBuildFile();
        }
    }

    void BuildFileCompilerNode::cleanOutputGroup(GroupNode* group) {
        bool updated = false;
        std::vector<std::shared_ptr<Node>> content = group->content();
        for (auto const& pair : _outputs) {
            auto it = std::find(content.begin(), content.end(), pair.second);
            if (it != content.end()) {
                content.erase(it);
                updated = true;
            }
        }
        if (updated) group->content(content);
    }

    void BuildFileCompilerNode::compileBuildFile() {
        try {
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> allowedInputs;
            for (auto const& pair : _depCompilers) {
                auto const& outputs = pair.second->_outputs;
                for (auto const& opair : pair.second->_outputs) {
                    allowedInputs.insert(opair);
                }
            }
            BuildFileCompiler compiler(
                context(),
                _buildFileParser->buildFileDirectory(),
                _buildFileParser->parseTree(),
                _commands,
                _outputs,
                _outputGroups,
                allowedInputs);
            updateMap(context(), this, _commands, compiler.commands());
            updateMap(context(), this, _outputGroups, compiler.outputGroups());
            updateMap(context(), this, _outputs, compiler.outputs());
            updateLineNrs(_ruleLineNrs, _commands, compiler.ruleLineNrs(), _buildFileParser->buildFile());
   
            if (validGeneratedInputs()) {
                XXH64_hash_t oldHash = _executionHash;
                _executionHash = computeExecutionHash();
                if (_executionHash != oldHash) modified(true);
                if (_executionHash != oldHash && context()->logBook()->mustLogAspect(LogRecord::Aspect::FileChanges)) {
                    std::stringstream ss;
                    ss << className() << " " << name().string() << " has compiled because of changed parseTree/glob deps/buildfile deps.";
                    LogRecord change(LogRecord::FileChanges, ss.str());
                    context()->logBook()->add(change);
                }
                notifyProcessingCompletion(Node::State::Ok);
            } else {
                _executionHash = rand();
                modified(true);
                notifyProcessingCompletion(Node::State::Failed);
            }
        } catch (std::runtime_error e) {
            _executionHash = rand();
            modified(true);
            LogRecord error(LogRecord::Error, e.what());
            context()->logBook()->add(error);
            notifyProcessingCompletion(Node::State::Failed);
        }
    }

    BuildFileParserNode const* BuildFileCompilerNode::findDefiningParser(GeneratedFileNode const* genFile) const {
        auto thisBuildFile = _buildFileParser->buildFile().get();
        SourceFileNode const* definingBuildFile = genFile->producer()->buildFile();
        auto const& parsers = _buildFileParser->dependencies();
        BuildFileParserNode const* definingParser = _buildFileParser.get();
        if (thisBuildFile != definingBuildFile) {
            definingParser = findParser(parsers, definingBuildFile);
        }
        if (definingParser == nullptr) {
            std::stringstream ss;
            ss << "Buildfile " << thisBuildFile->name()
                << " references generated input file " << genFile->name() << std::endl;
            ss << "which is defined in buildfile " << definingBuildFile->name() << std::endl;
            ss << definingBuildFile->name() << " must be declared as buildfile dependency in buildfile " << _buildFileParser->buildFile()->name();
            ss << std::endl;
            LogRecord error(LogRecord::Error, ss.str());
            context()->logBook()->add(error);
        }
        return definingParser;
    }

    bool BuildFileCompilerNode::findDefiningParsers(
        std::vector<std::shared_ptr<Node>> const& inputs,
        std::unordered_set<BuildFileParserNode const*>& parsers
    ) const {
        bool valid = true;
        for (auto const& input : inputs) {
            auto genFile = dynamic_cast<GeneratedFileNode const*>(input.get());
            if (genFile != nullptr) {
                auto parser = findDefiningParser(genFile);
                valid = nullptr != parser;
                if (valid && parser != _buildFileParser.get()) parsers.insert(parser);
            }
        }
        return valid;
    }

    bool BuildFileCompilerNode::validGeneratedInputs() const {
        if (_buildFileParser->buildFile() == nullptr) return true;
        std::unordered_set<BuildFileParserNode const*> usedParsers;
        bool valid = true;
        for (auto const& pair : _commands) {
            std::shared_ptr<CommandNode> const& cmd = pair.second;
            if (!findDefiningParsers(cmd->cmdInputs(), usedParsers)) valid = false;
            if (!findDefiningParsers(cmd->orderOnlyInputs(), usedParsers)) valid = false;
        }
        logNotUsedParserDependencies(usedParsers);
        return valid;
    }

    void BuildFileCompilerNode::logNotUsedParserDependencies(
        std::unordered_set<BuildFileParserNode const*> const& usedParsers
    ) const {
        std::vector<BuildFileParserNode const*> notUsedParsers;
        findNotUsedParsers(_buildFileParser->dependencies(), usedParsers, notUsedParsers); 
        if (!notUsedParsers.empty()) {
            auto thisBuildFile = _buildFileParser->buildFile().get();
            std::stringstream ss;
            ss << "Buildfile " << thisBuildFile->name() << std::endl;
            if (notUsedParsers.size() == 1) {
                auto p = *(notUsedParsers.begin());
                ss << "declares a not-used buildfile dependency on: " << p->buildFile()->name() << std::endl;
            } else {
                ss << "declares not-used buildfile dependencies on: " << std::endl;
                for (auto nu : notUsedParsers) ss << "\t" << nu->buildFile()->name() << std::endl;
            }
            ss << "Not-used buildfile dependencies may slowdown your build." << std::endl;
            LogRecord warning(LogRecord::Warning, ss.str());
            context()->logBook()->add(warning);
        }
    }

    void BuildFileCompilerNode::notifyProcessingCompletion(Node::State state) {
        Node::notifyCompletion(state);
    }

    void BuildFileCompilerNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        for (auto const& pair : _commands) outputs.push_back(pair.second);
    }

    void BuildFileCompilerNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
        for (auto const& pair : _depCompilers) inputs.push_back(pair.second);
        for (auto const& pair : _depGlobs) inputs.push_back(pair.second);
    }

    void BuildFileCompilerNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t BuildFileCompilerNode::typeId() const {
        return streamableTypeId;
    }

    void BuildFileCompilerNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(_buildFileParser);
        NodeMapStreamer::stream(streamer, _depCompilers);
        NodeMapStreamer::stream(streamer, _depGlobs);
        NodeMapStreamer::stream(streamer, _commands);
        NodeMapStreamer::stream(streamer, _outputs);
        NodeMapStreamer::stream(streamer, _outputGroups);
        streamer->streamVector(_ruleLineNrs);
        streamer->stream(_executionHash);
    }

    void BuildFileCompilerNode::prepareDeserialize() {
        Node::prepareDeserialize();
        _buildFileParser->removeObserver(this);
        for (auto const& i : _depCompilers) i.second->removeObserver(this);
        for (auto const& i : _depGlobs) i.second->removeObserver(this);
        _depCompilers.clear();
        _depGlobs.clear();
        _commands.clear();
        _outputs.clear();
        _outputGroups.clear();
        _ruleLineNrs.clear();

    }
    bool BuildFileCompilerNode::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!Node::restore(context, restored)) return false;
        if (_buildFileParser != nullptr) {
            _buildFileParser->restore(context, restored);
            _buildFileParser->addObserver(this);
        }
        NodeMapStreamer::restore(_depCompilers);
        NodeMapStreamer::restore(_depGlobs);
        NodeMapStreamer::restore(_commands);
        NodeMapStreamer::restore(_outputs);
        NodeMapStreamer::restore(_outputGroups);
        for (auto const& i : _depCompilers) {
            auto const& compiler = i.second;
            compiler->restore(context, restored);
            compiler->addObserver(this);
        }
        for (auto const& i : _depGlobs) {
            auto const& glob = i.second;
            glob->restore(context, restored);
            glob->addObserver(this);
        }
        std::size_t ruleIndex = 0;
        for (auto const& i : _commands) {
            auto const& cmd = i.second;
            cmd->restore(context, restored);
            cmd->buildFile(_buildFileParser->buildFile().get());
            cmd->ruleLineNr(_ruleLineNrs[ruleIndex]);
            ruleIndex++;
        }
        return true;
    }

}