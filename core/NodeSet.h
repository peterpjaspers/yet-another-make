#pragma once

#include <map>
#include <filesystem>
#include <memory>

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
		void addIfAbsent(std::shared_ptr<Node> node);

		// Add node to the set.
		// Pre: !Contains(node->Name())
		void add(std::shared_ptr<Node> node);

		// Remove node from the set.
		// Pre: Contains(node->Name())
		void remove(std::shared_ptr<Node> node);

		// Remove node from the set.
		void removeIfPresent(std::shared_ptr<Node> node);

		// Remove all nodes from the set.
		void clear();

		// Find and return node that matches nodeName;
		// Return null when not found.
		std::shared_ptr<Node> find(std::filesystem::path const& nodeName) const;

		// Return whether the set contains a node with given 'nodeName'
		bool contains(std::filesystem::path const& nodeName) const;

		std::size_t size() const;

		std::map<std::filesystem::path, std::shared_ptr<Node> > const& nodes() const;

	private:
		// TODO: used unordered_set. However set cannot use std::filesystem::path as keys.
		std::map<std::filesystem::path, std::shared_ptr<Node> > _nodes;
	};
}

