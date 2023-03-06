#include "Node.h"
#include "NodeSet.h"

namespace YAM
{
    void NodeSet::addIfAbsent(std::shared_ptr<Node> node) {
        std::lock_guard<std::mutex> lock(_mutex);
        const auto notUsed = _nodes.insert({ node->name(), node });
    }

    void NodeSet::add(std::shared_ptr<Node> node) {
        std::lock_guard<std::mutex> lock(_mutex);
        const auto [it, success] = _nodes.insert({ node->name(), node });
        if (!success) throw std::runtime_error("failed to add node");
    }

    void NodeSet::remove(std::shared_ptr<Node> node) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto nRemoved = _nodes.erase(node->name());
        if (nRemoved != 1) throw std::runtime_error("failed to remove node");
    }

    void NodeSet::removeIfPresent(std::shared_ptr<Node> node) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto notUsed = _nodes.erase(node->name());
    }

    void NodeSet::clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _nodes.clear();
    }

    std::shared_ptr<Node> NodeSet::find(std::filesystem::path const& nodeName)
    {
        std::lock_guard<std::mutex> lock(_mutex);
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
    ) {
        foundNodes.clear();
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto const& pair : _nodes) {
            if (includeNode.Execute(pair.second)) {
                foundNodes.push_back(pair.second);
            }
        }
    }

    bool NodeSet::contains(std::filesystem::path const& nodeName)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _nodes.find(nodeName);
        return (it != _nodes.end());
    }

    std::size_t NodeSet::size() { 
        std::lock_guard<std::mutex> lock(_mutex);
        return _nodes.size(); 
    }
}