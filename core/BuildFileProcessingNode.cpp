#include "BuildFileProcessingNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "DirectoryNode.h"
#include "CommandNode.h"
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

    void removeCommand(ExecutionContext* context, std::shared_ptr<CommandNode> cmd) {
        static std::vector<std::shared_ptr<GeneratedFileNode>> emptyOutputs;

        std::vector<std::shared_ptr<GeneratedFileNode>> outputs = cmd->outputs();
        cmd->outputs(emptyOutputs);
        for (auto const& output : outputs) {
            context->nodes().remove(output);
        }
        context->nodes().remove(cmd);
    }
    template<class T>
    void computeSetsDifference(
        std::unordered_set<T> const& in1,
        std::unordered_set<T> const& in2,
        std::unordered_set<T>& inBoth,
        std::unordered_set<T>& onlyIn1,
        std::unordered_set<T>& onlyIn2
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
}

namespace YAM
{

    BuildFileProcessingNode::BuildFileProcessingNode() {}

    BuildFileProcessingNode::BuildFileProcessingNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
        , _buildFileExecutor(std::make_shared<CommandNode>(context, "executor"))
        , _buildFileHash(rand())
    {
        _buildFileExecutor->addObserver(this);
    }

    BuildFileProcessingNode::~BuildFileProcessingNode() {
        _buildFileExecutor->removeObserver(this);
    }

    void BuildFileProcessingNode::buildFile(std::shared_ptr<SourceFileNode> const& newFile) {
        if (_buildFile != newFile) {
            if (_buildFile != nullptr) {
                for (auto const& cmd : _commands) removeCommand(context(), cmd);
                _commands.clear();
            }
            _buildFile = newFile;
            setState(Node::State::Dirty);
        }
    }

    std::shared_ptr<SourceFileNode> BuildFileProcessingNode::buildFile() const {
        return _buildFile;
    }

    void BuildFileProcessingNode::setupBuildFileExecutor() {
        _rulesFile = FileSystem::createUniqueDirectory() / "rules.txt";

        std::stringstream ss;
        ss << _buildFile->name().filename() << " > " << _rulesFile.string();
        std::string script = ss.str();
        _buildFileExecutor->script(script);

        auto wdir = _buildFile->name().parent_path();
        auto wdirNode = dynamic_pointer_cast<DirectoryNode>(context()->nodes().find(wdir));
        _buildFileExecutor->workingDirectory(wdirNode);
    }
    
    void BuildFileProcessingNode::teardownBuildFileExecutor() {
        _buildFileExecutor->workingDirectory(nullptr);
        _buildFileExecutor->script(std::string());
        std::filesystem::remove_all(_rulesFile.parent_path());
        _rulesFile = "";
    }

    void BuildFileProcessingNode::start() {
        Node::start();
        if (_buildFile == nullptr) {
            postCompletion(Node::State::Ok);
            return;
        }

        std::vector<Node*> requisites;
        // 1. execute buildfile => output to temporary file
        //    if (output hash not changed) => finish
        // 2. parse buildfile output => syntaxtree + buildfile dependencies
        // 3. delete temporary file
        // 4. execute buildfile dependencies => refd generated files
        // 5. evaluate syntax tree => updated set of command nodes

        setupBuildFileExecutor();
        requisites.push_back(_buildFileExecutor.get());
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State state) { handleBuildFileExecutorCompletion(state); }
        );
        startNodes(requisites, std::move(callback));
    }

    void BuildFileProcessingNode::handleBuildFileExecutorCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            notifyProcessingCompletion(state);
        } else if (canceling()) {
            notifyProcessingCompletion(Node::State::Canceled);
        } else if (_buildFile->hashOf(FileAspect::entireFileAspect().name()) != _buildFileHash) {
            context()->statistics().registerSelfExecuted(this);
            auto d = Delegate<void>::CreateLambda(
                [this]() { parseBuildFile(); }
            );
            context()->threadPoolQueue().push(std::move(d));
        } else {
            notifyProcessingCompletion(Node::State::Canceled);
        }
    }

    // threadpool
    void BuildFileProcessingNode::parseBuildFile() {
        std::shared_ptr<BuildFile::File> buildFile;
        std::string errorMsg;
        try {
            BuildFileParser parser(_rulesFile);
            buildFile = parser.file();
        } catch (std::runtime_error ex) {
            errorMsg = ex.what();
        }
        auto d = Delegate<void>::CreateLambda(
            [this, buildFile, errorMsg]() { handleParseBuildFileCompletion(buildFile, errorMsg); }
        );
        context()->mainThreadQueue().push(std::move(d));
    }

    void BuildFileProcessingNode::handleParseBuildFileCompletion(std::shared_ptr<BuildFile::File> file, std::string error) {
        if (!error.empty()) {
            LogRecord record(LogRecord::Aspect::Error, error);
            context()->logBook()->add(record);
            notifyProcessingCompletion(Node::State::Failed);
        } else {
            BuildFileCompiler compiler(context(), _buildFileExecutor->workingDirectory(), file);
            auto newCommands = compiler.commands();

            std::unordered_set<std::shared_ptr<CommandNode>> newSet(newCommands.begin(), newCommands.end());
            std::unordered_set<std::shared_ptr<CommandNode>> oldSet(_commands.begin(), _commands.end());
            std::unordered_set<std::shared_ptr<CommandNode>> keptCmds;
            std::unordered_set<std::shared_ptr<CommandNode>> newCmds;
            std::unordered_set<std::shared_ptr<CommandNode>> removedCmds;
            computeSetsDifference(
                newSet,
                oldSet,
                keptCmds,
                newCmds,
                removedCmds);
            for (auto const& cmd : newCmds) context()->nodes().add(cmd);
            for (auto const& cmd : removedCmds) cmd->setState(Node::State::Deleted);
            _commands = newCommands;
        }
        notifyProcessingCompletion(Node::State::Ok);
    }

    void BuildFileProcessingNode::compile(std::shared_ptr<BuildFile::File> const& file) {
    }

    void BuildFileProcessingNode::notifyProcessingCompletion(Node::State state) {
        teardownBuildFileExecutor();
        Node::notifyCompletion(state);

    }

    void BuildFileProcessingNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        for (auto const& cmd : _commands) {
            outputs.push_back(cmd);
        }
    }

    void BuildFileProcessingNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
        for (auto const& pair : _inputs) {
            inputs.push_back(pair.second);
        }
    }

    void BuildFileProcessingNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t BuildFileProcessingNode::typeId() const {
        return streamableTypeId;
    }

    void BuildFileProcessingNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
    }

    void BuildFileProcessingNode::prepareDeserialize() {
        Node::prepareDeserialize();

    }
    void BuildFileProcessingNode::restore(void* context) {
        Node::restore(context);
    }

}