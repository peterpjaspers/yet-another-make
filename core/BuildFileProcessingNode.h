#pragma once
#include "Node.h"
#include "../xxHash/xxhash.h"

#include <map>
#include <memory>
#include <set>

namespace YAM {
    class SourceFileNode;
    class CommandNode;

    class __declspec(dllexport) BuildFileProcessingNode : public Node
    {
    public:
        BuildFileProcessingNode(); // needed for deserialization
        BuildFileProcessingNode(ExecutionContext* context, std::filesystem::path const& name);
        virtual ~BuildFileProcessingNode();

        void buildFile(std::shared_ptr<SourceFileNode> const& newFile);
        std::shared_ptr<SourceFileNode> buildFile() const;

        // Inherited from Node
        void start() override;
        void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;
        void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamer (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        void restore(void* context) override;

    private:
        void setupBuildFileExecutor();
        void teardownBuildFileExecutor();
        void handleBuildFileExecutorCompletion(Node::State state);

        std::shared_ptr<SourceFileNode> _buildFile; 
        std::shared_ptr<CommandNode> _buildFileExecutor;
        std::filesystem::path _rulesFile;
        std::map<std::filesystem::path, std::shared_ptr<Node>> _inputs;
        std::set<std::shared_ptr<CommandNode>> _commands;
    };
}

