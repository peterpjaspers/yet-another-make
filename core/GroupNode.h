#pragma once
#include "Node.h"
#include "../xxHash/xxhash.h"

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

        std::string className() const override { return "GroupNode"; }

        void content(std::vector<std::shared_ptr<Node>> newContent);
        std::vector<std::shared_ptr<Node>> const& content() const { return _content; }

        // Override Node
        void start() override;
        void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;

        // Return a hash of the names of the nodes in the group.
        XXH64_hash_t hash() const { return _hash; }

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private:
        XXH64_hash_t computeHash() const;
        void handleGroupCompletion(Node::State groupState);

        std::vector<std::shared_ptr<Node>> _content;
        XXH64_hash_t _hash;
    };
}

