#include "BuildFileProcessingNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "DirectoryNode.h"
#include "GeneratedFileNode.h"
#include "CommandNode.h"
#include "GlobNode.h"
#include "ExecutionContext.h"
#include "FileSystem.h"
#include "FileAspect.h"
#include "BuildFileParser.h"
#include "BuildFileCompiler.h"

#include <vector>
#include <unordered_set>


namespace
{
    using namespace YAM;

    uint32_t streamableTypeId = 0;

    void removeNode(std::shared_ptr<CommandNode> cmd) {
        static std::vector<std::shared_ptr<GeneratedFileNode>> emptyOutputs;

        ExecutionContext* context = cmd->context();
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs = cmd->outputs();
        cmd->outputs(emptyOutputs);
        for (auto const& output : outputs) {
            output->setState(Node::State::Deleted);
            context->nodes().remove(output);
        }
        cmd->setState(Node::State::Deleted);
    }

    void removeNode(std::shared_ptr<GlobNode> glob) {
        glob->setState(Node::State::Deleted);
    }

    void removeNode(std::shared_ptr<GeneratedFileNode> node) {
        node->setState(Node::State::Deleted);
    }

    template<class TNode>
    void computeUnorderedSetsDifference(
        std::unordered_set<std::shared_ptr<TNode>> const& in1,
        std::unordered_set<std::shared_ptr<TNode>> const& in2,
        std::unordered_set<std::shared_ptr<TNode>>& inBoth,
        std::unordered_set<std::shared_ptr<TNode>>& onlyIn1,
        std::unordered_set<std::shared_ptr<TNode>>& onlyIn2
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

    template<class TNode>
    void computeMapsDifference(
        std::map<std::filesystem::path, std::shared_ptr<TNode>> const& in1,
        std::map<std::filesystem::path, std::shared_ptr<TNode>> const& in2,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& inBoth,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& onlyIn1,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& onlyIn2
    ) {
        for (auto i1 : in1) {
            if (in2.find(i1.first) != in2.end()) inBoth.insert(i1);
            else onlyIn1.insert(i1);
        }
        for (auto i2 : in2) {
            if (in1.find(i2.first) != in1.end()) onlyIn2.insert(i2);
        }
    }

    template <class TNode>
    void updateMap(
        ExecutionContext* context,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& toUpdate,
        std::map<std::filesystem::path, std::shared_ptr<TNode>> const& newSet
    ) {
        std::map<std::filesystem::path, std::shared_ptr<TNode>> kept;
        std::map<std::filesystem::path, std::shared_ptr<TNode>> added;
        std::map<std::filesystem::path, std::shared_ptr<TNode>> removed;
        computeMapsDifference(newSet, toUpdate, kept, added, removed);
        for (auto const& pair : added) context->nodes().add(pair.second);
        for (auto const& pair : removed) removeNode(pair.second);
        toUpdate = newSet;
    }
}

namespace YAM
{

    BuildFileProcessingNode::BuildFileProcessingNode() {}

    BuildFileProcessingNode::BuildFileProcessingNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
        , _buildFileExecutor(std::make_shared<CommandNode>(context, "executor"))
        , _executionHash(rand())
    {
    }

    BuildFileProcessingNode::~BuildFileProcessingNode() {
        for (auto const& pair : _depFiles) pair.second->removeObserver(this);
        for (auto const& pair : _depBFPNs) pair.second->removeObserver(this);
        for (auto const& pair : _depGlobs) pair.second->removeObserver(this);
    }

    void BuildFileProcessingNode::buildFile(std::shared_ptr<SourceFileNode> const& newFile) {
        if (_buildFile != newFile) {
            if (_buildFile != nullptr) {
                for (auto const& pair : _depFiles) pair.second->removeObserver(this);
                for (auto const& pair : _depBFPNs) pair.second->removeObserver(this);
                for (auto const& pair : _depGlobs) pair.second->removeObserver(this);
                for (auto pair : _commands) removeNode(pair.second);
                for (auto pair : _depGlobs) removeNode(pair.second);
                _commands.clear();
                _depFiles.clear();
                _depBFPNs.clear();
                _depGlobs.clear();
            }
            _buildFile = newFile;
            setState(Node::State::Dirty);
        }
    }

    std::shared_ptr<SourceFileNode> BuildFileProcessingNode::buildFile() const {
        return _buildFile;
    }

    void BuildFileProcessingNode::setupBuildFileExecutor() {
        _tmpRulesFile = FileSystem::createUniqueDirectory() / "rules.txt";

        std::stringstream ss;
        ss << _buildFile->name().filename() << " > " << _tmpRulesFile.string();
        std::string script = ss.str();
        _buildFileExecutor->script(script);

        auto wdir = _buildFile->name().parent_path();
        auto wdirNode = dynamic_pointer_cast<DirectoryNode>(context()->nodes().find(wdir));
        _buildFileExecutor->workingDirectory(wdirNode);
        _buildFileExecutor->addObserver(this);
    }
    
    void BuildFileProcessingNode::teardownBuildFileExecutor() {
        _buildFileExecutor->removeObserver(this);
        _buildFileExecutor->workingDirectory(nullptr);
        _buildFileExecutor->script(std::string());
        std::filesystem::remove_all(_tmpRulesFile.parent_path());
        _tmpRulesFile = "";
    }

    XXH64_hash_t BuildFileProcessingNode::computeExecutionHash() const {
        std::vector<XXH64_hash_t> hashes;
        for (auto pair : _depFiles) hashes.push_back(pair.second->hashOf(FileAspect::entireFileAspect().name()));
        for (auto pair : _depBFPNs) hashes.push_back(pair.second->_executionHash);
        for (auto pair : _depGlobs) hashes.push_back(pair.second->executionHash());
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    void BuildFileProcessingNode::start() {
        Node::start();
        if (_buildFile == nullptr) {
            postCompletion(Node::State::Ok);
            return;
        }
        // 1. execute requisites: 
        //    _buildFileExecutor, _depFiles, _depBFPNs, _depGlobs
        // 2. If _requisitesHash == computeRequisitesHash()
        //    nothing changed => finish
        // 3. Execute _buildFileExecutor, redirect output to tmp file
        //    _depFiles = _buildFileExecutor.getInputFiles() 
        // 4. parse temp file => BuildFile::File
        // 3. TODO: 
        //    Extract from parse result the deps on buildfiles and globs.
        //    Execute these dependencies, update _depBFPNs and _depGlobs.
        // 5. compile BuildFile::File => update _commands and _ruleGlobs

        setupBuildFileExecutor();
        std::vector<std::shared_ptr<Node>> requisites;
        requisites.push_back(_buildFileExecutor);
        getInputs(requisites);

        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State state) { handleRequisitesCompletion(state); }
        );
        startNodes(requisites, std::move(callback));
    }

    void BuildFileProcessingNode::handleRequisitesCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            notifyProcessingCompletion(state);
        } else if (canceling()) {
            notifyProcessingCompletion(Node::State::Canceled);
        } else {
            for (auto const& pair : _depFiles) pair.second->removeObserver(this);
            _depFiles = _buildFileExecutor->detectedInputs();
            for (auto const& pair : _depFiles) pair.second->addObserver(this);

            if (_executionHash == computeExecutionHash()) {
                notifyProcessingCompletion(Node::State::Ok);
            } else {
                context()->statistics().registerSelfExecuted(this);
                auto d = Delegate<void>::CreateLambda(
                    [this]() { parseBuildFile(); }
                );
                context()->threadPoolQueue().push(std::move(d));
            }
        }
    }

    // threadpool
    void BuildFileProcessingNode::parseBuildFile() {
        std::string errorMsg;
        try {
            BuildFileParser parser(_tmpRulesFile);
            _parseTree = parser.file();
        } catch (std::runtime_error ex) {
            errorMsg = ex.what();
        }
        auto d = Delegate<void>::CreateLambda(
            [this, errorMsg]() { handleParseBuildFileCompletion(errorMsg); }
        );
        context()->mainThreadQueue().push(std::move(d));
    }

    void BuildFileProcessingNode::handleParseBuildFileCompletion(std::string error) {
        if (!error.empty()) {
            LogRecord record(LogRecord::Aspect::Error, error);
            context()->logBook()->add(record);
            notifyProcessingCompletion(Node::State::Failed);
        } else {
            // TODO: extract buildfile dependencies from file and execute
            // associated BFPNs first
            try {
                BuildFileCompiler compiler(context(), _buildFileExecutor->workingDirectory(), *_parseTree);
                updateMap<GeneratedFileNode>(context(), _outputs, compiler.outputs());
                updateMap<CommandNode>(context(), _commands, compiler.commands());

                for (auto const& pair : _depGlobs) pair.second->removeObserver(this);
                updateMap<GlobNode>(context(), _depGlobs, compiler.globs());
                for (auto const& pair : _depGlobs) pair.second->addObserver(this);

                _executionHash = computeExecutionHash();
            } catch (std::runtime_error e) {
                auto msg = e.what();
                notifyProcessingCompletion(Node::State::Failed);
            }
        }
        notifyProcessingCompletion(Node::State::Ok);
    }

    void BuildFileProcessingNode::handleDepBFPNsCompletion(Node::State state) {
        if (state == Node::State::Failed) {
            notifyProcessingCompletion(Node::State::Failed);
        } else {
            try {
                BuildFileCompiler compiler(context(), _buildFileExecutor->workingDirectory(), *_parseTree);
                updateMap<GeneratedFileNode>(context(), _outputs, compiler.outputs());
                updateMap<CommandNode>(context(), _commands, compiler.commands());

                for (auto const& pair : _depGlobs) pair.second->removeObserver(this);
                updateMap<GlobNode>(context(), _depGlobs, compiler.globs());
                for (auto const& pair : _depGlobs) pair.second->addObserver(this);

                _executionHash = computeExecutionHash();
            } catch (std::runtime_error e) {
                auto msg = e.what();
                notifyProcessingCompletion(Node::State::Failed);
            }
        }
        notifyProcessingCompletion(Node::State::Ok);
    }

    void BuildFileProcessingNode::notifyProcessingCompletion(Node::State state) {
        teardownBuildFileExecutor();
        _parseTree = nullptr;
        Node::notifyCompletion(state);

    }

    void BuildFileProcessingNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        for (auto const& pair : _commands) outputs.push_back(pair.second);
    }

    void BuildFileProcessingNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
        for (auto const& pair : _depFiles) inputs.push_back(pair.second);
        for (auto const& pair : _depBFPNs) inputs.push_back(pair.second);
        for (auto const& pair : _depGlobs) inputs.push_back(pair.second);
    }

    void BuildFileProcessingNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t BuildFileProcessingNode::typeId() const {
        return streamableTypeId;
    }

    void BuildFileProcessingNode::stream(IStreamer* streamer) {
        throw std::runtime_error("not implemented");
        Node::stream(streamer);
    }

    void BuildFileProcessingNode::prepareDeserialize() {
        throw std::runtime_error("not implemented");
        Node::prepareDeserialize();

    }
    void BuildFileProcessingNode::restore(void* context) {
        throw std::runtime_error("not implemented");
        Node::restore(context);
    }

}