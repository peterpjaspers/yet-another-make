#pragma once
#include "Node.h"
#include "xxhash.h"

#include <vector>
#include <set>
#include <memory>

namespace YAM
{
    class CommandNode;
    class FileNode;

    // A GroupNode facilitates the execution of a collection of nodes of 
    // arbitrary type: executing a group will execute all elements.
    // Special behavior is provided for GeneratedFileNodes: instead of 
    // executing a generated file node (which only hashes the file) the 
    // producer of a generated file node is executed (which makes the generated
    // file up-to-date and hashes it).
    // 
    class __declspec(dllexport) GroupNode : public Node
    {
    public:
        GroupNode(); // needed for deserialization
        GroupNode(ExecutionContext* context, std::filesystem::path const& name);

        std::string className() const override { return "GroupNode"; }

        // Replace content by newContent.
        void content(std::vector<std::shared_ptr<Node>> newContent);
        // Pre: !content().contains(node)
        void add(std::shared_ptr<Node> const& node);
        // Pre: content().contains(node)
        void remove(std::shared_ptr<Node> const& node);
        // Remove node if content().contains(node). 
        // Return whether node was present.
        bool removeIfPresent(std::shared_ptr<Node> const& node);

        // Return the content as defined by use of content(newContent), 
        // add(node), remove(node), removeNodIfPresent(node) functions.
        std::set<std::shared_ptr<Node>, Node::CompareName> const& content() const { return _content; }

        // Return the union of the elements in content() that are FileNodes 
        // and the mandatory and detected optional output nodes of elements
        // that are CommandNodes.
        std::set<std::shared_ptr<FileNode>, Node::CompareName> files() const;

        // Override Node
        void start(PriorityClass prio) override;

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

        // A set instead of a map from name->node is used to reduce memory 
        // usage of groups with large number elements.
        std::set<std::shared_ptr<Node>, Node::CompareName> _content;

        // _contentVec is used to stream _content. When streaming nodes from
        // persistent repository the nodes are constructed but their members 
        // (a.o. their name) may not yet have been streamed. Because _content
        // is sorted by node name first all nodes must be streamed. This implies
        // that only during restore() the nodes can be added to _content. 
        std::vector<std::shared_ptr<Node>> _contentVec;

        // _observed contains the nodes that are being observed.
        // For a GeneratedFileNode X->producer() is observed. When a set of N
        // GeneratedFileNodes have same producer C then _observed[C]==N.
        // For a node X not being a GeneratedFileNode X is observed and
        // _observed[X]==1
        std::unordered_map<Node*, uint32_t> _observed;
        XXH64_hash_t _hash;
    };
}

