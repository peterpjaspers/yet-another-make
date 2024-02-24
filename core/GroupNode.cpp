#include "GroupNode.h"
#include "CommandNode.h"
#include "GeneratedFileNode.h"
#include "ExecutionContext.h"
#include "IStreamer.h"

#include <unordered_set>

namespace
{
    using namespace YAM;

    uint32_t streamableTypeId = 0;

    Node* getProducer(Node* node) {
        auto genFileNode = dynamic_cast<GeneratedFileNode*>(node);
        if (genFileNode != nullptr) {
            return genFileNode->producer().get();
        }
        return node;
    }

    void subscribe(std::vector<std::shared_ptr<Node>> const& nodes, StateObserver* observer) {
        std::unordered_set<Node*> subscribed;
        for (auto const& node : nodes) {
            Node* producer = getProducer(node.get());
            if (subscribed.insert(producer).second) producer->addObserver(observer);
        }
    }

    void unsubscribe(std::vector<std::shared_ptr<Node>> const& nodes, StateObserver* observer) {
        std::unordered_set<Node*> unsubscribed;
        for (auto const& node : nodes) {
            Node* producer = getProducer(node.get());
            if (unsubscribed.insert(producer).second) producer->removeObserver(observer);
        }
    }
}

namespace YAM
{
    GroupNode::GroupNode() {}
    GroupNode::GroupNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
    {}

    void GroupNode::content(std::vector<std::shared_ptr<Node>> const &newContent) {
        Node::CompareName cmp;
        if (_content != newContent) {
            unsubscribe(_content, this);
            _content = newContent;
            std::sort(_content.begin(), _content.end(), cmp);
            subscribe(_content, this);
            modified(true);
            setState(Node::State::Dirty);
        }
    }

    void GroupNode::start() {
        Node::start();
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State state) { handleGroupCompletion(state); }
        );
        std::vector<Node*> requisites;
        for (auto const& node : _content) {
            auto genFileNode = dynamic_cast<GeneratedFileNode*>(node.get());
            if (genFileNode != nullptr) {
                requisites.push_back(genFileNode->producer().get());
            } else {
                requisites.push_back(node.get());
            }
        }
        startNodes(requisites, std::move(callback));
    }

    void GroupNode::handleGroupCompletion(Node::State groupState) {
        if (groupState == Node::State::Ok) {
            XXH64_hash_t prevHash = _hash;
            _hash = computeHash();
            if (prevHash != _hash) modified(true);
            if (prevHash != _hash && context()->logBook()->mustLogAspect(LogRecord::Aspect::DirectoryChanges)) {
                std::stringstream ss;
                ss << className() << " " << name().string() << " has changed.";
                LogRecord change(LogRecord::DirectoryChanges, ss.str());
                context()->logBook()->add(change);
            }
        }
        Node::notifyCompletion(groupState);
    }

    XXH64_hash_t GroupNode::computeHash() const {
        std::vector<XXH64_hash_t> hashes;
        for (auto const& n : _content) hashes.push_back(XXH64_string(n->name().string()));
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    void GroupNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        for (auto const& node : _content) {
            auto const& groupNode = dynamic_pointer_cast<GroupNode>(node);
            if (groupNode != nullptr) {
                groupNode->getOutputs(outputs);
            } else {
                outputs.push_back(node);
            }
        }
    }

    void GroupNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t GroupNode::typeId() const {
        return streamableTypeId;
    }

    void GroupNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->streamVector(_content);
        streamer->stream(_hash);
    }

    void GroupNode::prepareDeserialize() {
        static std::vector<std::shared_ptr<Node>> empty;
        Node::prepareDeserialize();
        unsubscribe(_content, this);
        _content.clear();
    }

    bool GroupNode::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!Node::restore(context, restored)) return false;
        for (auto const& node : _content) node->restore(context, restored);
        subscribe(_content, this);
        return true;
    }
}

