#include "Node.h"
#include "NodeSet.h"

namespace YAM
{
    void NodeSet::addIfAbsent(std::shared_ptr<Node> const& node) {
        const auto notUsed = _nodes.insert({ node->name(), node });
    }

    void NodeSet::add(std::shared_ptr<Node> const& node) {
        const auto [it, success] = _nodes.insert({ node->name(), node });
        if (!success) throw std::runtime_error("failed to add node");
    }

    void NodeSet::remove(std::shared_ptr<Node> const& node) {
        auto nRemoved = _nodes.erase(node->name());
        if (nRemoved != 1) throw std::runtime_error("failed to remove node");
    }

    void NodeSet::removeIfPresent(std::shared_ptr<Node> const& node) {
        auto notUsed = _nodes.erase(node->name());
    }

    void NodeSet::clear() {
        _nodes.clear();
    }

    std::shared_ptr<Node> NodeSet::find(std::filesystem::path const& nodeName) const {
        auto it = _nodes.find(nodeName);
        if (it != _nodes.end())
        {
            return it->second;
        } else {
            return std::shared_ptr<Node>();
        }
    }

    void NodeSet::find(
        Delegate<bool, std::shared_ptr<Node> const&> includeNode,
        std::vector<std::shared_ptr<Node>>& foundNodes
    ) const {
        foundNodes.clear();
        for (auto const& pair : _nodes) {
            if (includeNode.Execute(pair.second)) {
                foundNodes.push_back(pair.second);
            }
        }
    }

    void NodeSet::foreach(Delegate<void, std::shared_ptr<Node> const&> action) {
        for (auto const& pair : _nodes) {
            action.Execute(pair.second);
        }
    }

    bool NodeSet::contains(std::filesystem::path const& nodeName) const {
        auto it = _nodes.find(nodeName);
        return (it != _nodes.end());
    }

    std::size_t NodeSet::size() const {
        return _nodes.size(); 
    }
}