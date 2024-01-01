#pragma once
#include "Node.h"

#include <vector>
#include <memory>

namespace YAM
{
    // A GroupNode facilitates the execution of an arbitrary collection of
    // nodes.
    class __declspec(dllexport) GroupNode : public Node
    {
    public:
        GroupNode(); // needed for deserialization
        GroupNode(ExecutionContext* context, std::filesystem::path const& name);

        void group(std::vector<std::shared_ptr<Node>> newGroup);
        std::vector<std::shared_ptr<Node>> const& group() const { return _group; }

        // Override Node
        void start() override;

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private:
        void handleGroupCompletion(Node::State groupState);

        std::vector<std::shared_ptr<Node>> _group;
    };
}

