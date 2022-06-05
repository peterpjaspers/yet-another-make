#pragma once

#include <map>
#include <filesystem>

namespace YAM
{
	class Node;
	class Dispatcher;

	class __declspec(dllexport) NodeSet
	{
	public:
		// Construct a set that can hold Node instances.
		// Nodes are identified by their name(). 
		// No duplicates allowed.
		NodeSet() = default;

		// Add node to the set.
		void addIfAbsent(Node* node);

		// Add node to the set.
		// Pre: !Contains(node->Name())
		void add(Node* node);

		// Remove node from the set.
		// Pre: Contains(node->Name())
		void remove(Node* node);

		// Remove node from the set.
		void removeIfPresent(Node* node);

		// Find and return node that matches nodeName;
		// Return null when not found.
		Node* find(std::filesystem::path const& nodeName) const;

		// Return whether the set contains a node with given 'nodeName'
		bool contains(std::filesystem::path const& nodeName) const;

		std::size_t size() const;

	private:
		// TODO: used unordered_set. However set cannot use path as keys.
		std::map<std::filesystem::path, Node*> _nodes;
	};
}

