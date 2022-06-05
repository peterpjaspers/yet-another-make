#include "Node.h"
#include "NodeSet.h"

namespace YAM
{
	void NodeSet::addIfAbsent(Node* node) {
		const auto notUsed = _nodes.insert({ node->name(), node });
	}

	void NodeSet::add(Node* node) {
		const auto [it, success] = _nodes.insert({ node->name(), node });
		if (!success) throw std::runtime_error("failed to add node");
	}

	void NodeSet::remove(Node* node) {
		auto nRemoved = _nodes.erase(node->name());
		if (nRemoved != 1) throw std::runtime_error("failed to remove node");
	}

	void NodeSet::removeIfPresent(Node* node) {
		auto notUsed = _nodes.erase(node->name());
	}

	Node* NodeSet::find(std::filesystem::path const& nodeName) const
	{
		auto it = _nodes.find(nodeName);
		if (it != _nodes.end())
		{
			return it->second;
		} else {
			return nullptr;
		}
	}

	bool NodeSet::contains(std::filesystem::path const& nodeName) const
	{
		auto it = _nodes.find(nodeName);
		return (it != _nodes.end());
	}

	std::size_t NodeSet::size() const { return _nodes.size(); }
}