#pragma once

#include "Delegates.h"
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace YAM
{
    class Node;

    // Class to store nodes that have unique names.
    class __declspec(dllexport) NodeSet
    {
    public:
        // Construct a set that can hold Node instances.
        // Nodes are identified by their name(). 
        // No duplicates allowed.
        NodeSet() = default;

        // Add node to the set.
        void addIfAbsent(std::shared_ptr<Node> const& node);

        // Add node to the set.
        // Pre: !Contains(node->Name())
        void add(std::shared_ptr<Node> const& node);

        // Remove node from the set.
        // Pre: Contains(node->Name())
        void remove(std::shared_ptr<Node> const& node);

        // Remove node from the set.
        void removeIfPresent(std::shared_ptr<Node> const& node);

        // Remove all nodes from the set.
        void clear();

        // Find and return node that matches nodeName;
        // Return null when not found.
        std::shared_ptr<Node> find(std::filesystem::path const& nodeName) const;

        // Return in 'foundNodes' all nodes for which includeNode(node)==true.
        void find(
            Delegate<bool, std::shared_ptr<Node> const&> includeNode,
            std::vector<std::shared_ptr<Node>>& foundNodes) const;

        // Execute action on each node in the set.
        void foreach(Delegate<void, std::shared_ptr<Node> const&> action);

        // Return whether the set contains a node with given 'nodeName'
        bool contains(std::filesystem::path const& nodeName) const;

        std::size_t size() const;

        std::vector<std::shared_ptr<Node>> nodes() const;
        std::unordered_map<std::filesystem::path, std::shared_ptr<Node>> nodesMap() const;

        // Register node as modified in changeset.
        // Pre: node->modified()
        void changeSetModify(std::shared_ptr<Node> const& node);

        // Return changes in the nodeset since last call to clearChangeSet().
        // The sets do not intersect.
        std::unordered_set<std::shared_ptr<Node>> const& addedNodes() const;
        std::unordered_set<std::shared_ptr<Node>> const& modifiedNodes() const;
        std::unordered_set<std::shared_ptr<Node>> const& removedNodes() const;

        std::size_t changeSetSize() const;
        void clearChangeSet();

    private:
        void changeSetAdd(std::shared_ptr<Node> const& node);
        void changeSetRemove(std::shared_ptr<Node> const& node);

        std::unordered_map<std::filesystem::path, std::shared_ptr<Node>> _nodes;
        std::unordered_set<std::shared_ptr<Node>> _addedNodes;
        std::unordered_set<std::shared_ptr<Node>> _modifiedNodes;
        std::unordered_set<std::shared_ptr<Node>> _removedNodes;

    };
}

