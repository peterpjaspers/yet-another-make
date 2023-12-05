#include "GroupNode.h"

namespace YAM
{
    GroupNode::GroupNode() {}
    GroupNode::GroupNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
    {}

    void GroupNode::group(std::vector<std::shared_ptr<Node>> newGroup) {
        for (auto const& node : _group) {
            node->removeObserver(this);
        }
        _group = newGroup;
        for (auto const& node : _group) {
            node->addObserver(this);
        }
        setState(Node::State::Dirty);
    }

    void GroupNode::start() {
        Node::start();
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State state) { handleGroupCompletion(state); }
        );
        std::vector<Node*> group;
        for (auto const& n : _group) group.push_back(n.get());
        startNodes(group, std::move(callback));
    }

    void GroupNode::handleGroupCompletion(Node::State groupState) {
        Node::notifyCompletion(groupState);
    }
}

