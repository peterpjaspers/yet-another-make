#include "GroupNode.h"
#include "IStreamer.h"

namespace
{
    uint32_t streamableTypeId = 0;
}

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
        startNodes(_group, std::move(callback));
    }

    void GroupNode::handleGroupCompletion(Node::State groupState) {
        Node::notifyCompletion(groupState);
    }

    void GroupNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t GroupNode::typeId() const {
        return streamableTypeId;
    }
    void GroupNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->streamVector(_group);
    }
    void GroupNode::prepareDeserialize() {
        Node::prepareDeserialize();
        for (auto const& node : _group) node->removeObserver(this);
        _group.clear();
    }
    bool GroupNode::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!Node::restore(context, restored)) return false;
        for (auto const& node : _group) {
            node->restore(context, restored);
            node->addObserver(this);
        }
        return true;
    }
}

