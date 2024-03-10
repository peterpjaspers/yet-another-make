#pragma once
#include "Node.h"
#include "xxhash.h"

#include <vector>
#include <set>
#include <memory>

namespace YAM
{
    class CommandNode;

    // A GroupNode facilitates the execution of an arbitrary collection of
    // nodes.
    class __declspec(dllexport) GroupNode : public Node
    {
    public:
        GroupNode(); // needed for deserialization
        GroupNode(ExecutionContext* context, std::filesystem::path const& name);

        std::string className() const override { return "GroupNode"; }

        void content(std::vector<std::shared_ptr<Node>> newContent);
        // Pre: !contains(node)
        void add(std::shared_ptr<Node> const& node);
        // Pre: contains(node)
        void remove(std::shared_ptr<Node> const& node);
        // Return true when node was removed.
        bool removeIfPresent(std::shared_ptr<Node> const& node);

        std::set<std::shared_ptr<Node>, Node::CompareName> const& content() const { return _content; }
        bool contains(std::shared_ptr<Node> const& node) const;

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
        void subscribe(std::shared_ptr<Node> const& node);
        void unsubscribe(std::shared_ptr<Node> const& node);
        XXH64_hash_t computeHash() const;
        void handleGroupCompletion(Node::State groupState);

        std::set<std::shared_ptr<Node>, Node::CompareName> _content;

        // _contentVec is used to stream _content. When streaming nodes from
        // persistent repository the nodes are constructed but their members 
        // (a.o. their name) may not yet have been streamed. Because _content
        // is sorted by node name first all nodes must be streamed. This implies
        // that only during restore() the nodes can be added to _content. 
        std::vector<std::shared_ptr<Node>> _contentVec;

        // _observed contains the nodes that are being observed.
        // When a subset of N GeneratedFileNodes are output of same CommandNode
        // C then C is observed and _observed[C]==N.
        // For a node X not being a generated file node X itself is observed 
        // and _observed[X]==1
        std::unordered_map<Node*, uint32_t> _observed;
        XXH64_hash_t _hash;
    };
}

