#pragma once

#include "Delegates.h"
#include <filesystem>
#include <memory>
#include <unordered_map>

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
        std::shared_ptr<Node> find(std::filesystem::path const& nodeName);

        // Return in 'foundNodes' all nodes for which includeNode(node)==true.
        void find(
            Delegate<bool, std::shared_ptr<Node> const&> includeNode,
            std::vector<std::shared_ptr<Node>>& foundNodes);

        // Execute action on each node in the set.
        void foreach(Delegate<void, std::shared_ptr<Node> const&> action);

        // Return whether the set contains a node with given 'nodeName'
        bool contains(std::filesystem::path const& nodeName);

        std::size_t size();

    private:
        std::unordered_map<std::filesystem::path, std::shared_ptr<Node> > _nodes;
    };
}

