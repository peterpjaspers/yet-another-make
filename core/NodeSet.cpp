#include "Node.h"
#include "NodeSet.h"

namespace YAM
{
    void NodeSet::addIfAbsent(std::shared_ptr<Node> const& node) {
        const auto result = _nodes.insert({ node->name(), node });
        if (result.second) {
            if (node->state() == Node::State::Dirty) {
                registerDirtyNode(node);
            } else if (node->state() == Node::State::Failed || node->state() == Node::State::Canceled) {
                registerFailedOrCanceledNode(node);
            }
            changeSetAdd(node);
        }
    }

    void NodeSet::add(std::shared_ptr<Node> const& node) {
        const auto [it, success] = _nodes.insert({ node->name(), node });
        if (!success) throw std::runtime_error("failed to add node");
        if (node->state() == Node::State::Dirty) {
            registerDirtyNode(node);
        } else if (node->state() == Node::State::Failed || node->state() == Node::State::Canceled) {
            registerFailedOrCanceledNode(node);
        }
        changeSetAdd(node);
    }

    void NodeSet::remove(std::shared_ptr<Node> const& node) {
        auto nRemoved = _nodes.erase(node->name());
        if (nRemoved != 1) throw std::runtime_error("failed to remove node");
        node->setState(Node::State::Deleted);
        _dirtyNodes[node->className()].erase(node);
        changeSetRemove(node);
    }

    void NodeSet::removeIfPresent(std::shared_ptr<Node> const& node) {
        auto nRemoved = _nodes.erase(node->name());
        if (nRemoved == 1) {
            node->setState(Node::State::Deleted);
            _dirtyNodes[node->className()].erase(node);
            changeSetRemove(node);
        }
    }

    void NodeSet::clear() {
        for (auto const& pair : _nodes) changeSetRemove(pair.second);
        _dirtyNodes.clear();
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

    std::unordered_map<std::filesystem::path, std::shared_ptr<Node>> NodeSet::nodesMap() const {
        return _nodes;
    }

    std::vector<std::shared_ptr<Node>> NodeSet::nodes() const {
        std::vector<std::shared_ptr<Node>> nodes;
        for (auto const& pair : _nodes) nodes.push_back(pair.second);
        return nodes;
    }

    void NodeSet::NodeSet::registerDirtyNode(std::shared_ptr<Node> const& node) {
        if (!_nodes.contains(node->name())) return;
        auto result = _dirtyNodes[node->className()].insert(node);
        if (!result.second) throw std::runtime_error("Attempt to add duplicate dirty node");
    }

    void NodeSet::unregisterDirtyNode(std::shared_ptr<Node> const& node) {
        if (!_nodes.contains(node->name())) return;
        std::size_t nRemoved = _dirtyNodes[node->className()].erase(node);
        if (nRemoved != 1) throw std::runtime_error("Attempt to remove unknown dirty node");
    }

    std::unordered_map<std::string, std::unordered_set<std::shared_ptr<Node>>> const& NodeSet::dirtyNodes() const {
        return _dirtyNodes;
    }

    void NodeSet::NodeSet::registerFailedOrCanceledNode(std::shared_ptr<Node> const& node) {
        if (!_nodes.contains(node->name())) return;
        auto result = _failedOrCanceledNodes[node->className()].insert(node);
        if (!result.second) throw std::runtime_error("Attempt to add duplicate failed|canceled node");
    }

    void NodeSet::unregisterFailedOrCanceledNode(std::shared_ptr<Node> const& node) {
        if (!_nodes.contains(node->name())) return;
        std::size_t nRemoved = _failedOrCanceledNodes[node->className()].erase(node);
        if (nRemoved != 1) throw std::runtime_error("Attempt to remove unknown failed|canceled node");
    }

    std::unordered_map<std::string, std::unordered_set<std::shared_ptr<Node>>> const& NodeSet::failedOrCanceledNodes() const {
        return _failedOrCanceledNodes;
    }

    void NodeSet::changeSetModify(std::shared_ptr<Node> const& node) {
        if (_removedNodes.contains(node)) throw std::runtime_error("illegal change");
        if (!_addedNodes.contains(node)) _modifiedNodes.insert(node);
    }
    void NodeSet::changeSetAdd(std::shared_ptr<Node> const& node) {
        _modifiedNodes.erase(node);
        _addedNodes.insert(node);
    }
    void NodeSet::changeSetRemove(std::shared_ptr<Node> const& node) {
        _modifiedNodes.erase(node);
        _removedNodes.insert(node);
    }

    std::unordered_set<std::shared_ptr<Node>> const& NodeSet::addedNodes() const {
        return _addedNodes;
    }
    std::unordered_set<std::shared_ptr<Node>> const& NodeSet::modifiedNodes() const {
        return _modifiedNodes;
    }
    std::unordered_set<std::shared_ptr<Node>> const& NodeSet::removedNodes() const {
        return _removedNodes;
    }

    std::size_t NodeSet::changeSetSize() const {
        return _addedNodes.size() + _modifiedNodes.size() + _removedNodes.size();
    }
    void NodeSet::clearChangeSet() {
        _addedNodes.clear();
        _modifiedNodes.clear();
        _removedNodes.clear();
    }
}