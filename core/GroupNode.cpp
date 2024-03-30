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

    Node* getObservable(Node* node) {
        auto genFileNode = dynamic_cast<GeneratedFileNode*>(node);
        if (genFileNode != nullptr) {
            return genFileNode->producer().get();
        }
        return node;
    }
}

namespace YAM
{
    GroupNode::GroupNode() : Node(), _hash(rand()) {}
    GroupNode::GroupNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
        , _hash(rand())
    {}

    void GroupNode::content(std::vector<std::shared_ptr<Node>> newContent) {
        for (auto const& node : _content) unsubscribe(node);
        _content.clear();
        _content.insert(newContent.begin(), newContent.end());
        for (auto const& node : _content) subscribe(node);
        modified(true);
        setState(Node::State::Dirty);
    }

    void GroupNode::add(std::shared_ptr<Node> const& node) {
        auto result = _content.insert(node);
        if (!result.second) throw std::runtime_error("Attempt to add duplicate");
        subscribe(node);
        modified(true);
        setState(Node::State::Dirty);
    }

    void GroupNode::remove(std::shared_ptr<Node> const& node) {
        auto it = _content.find(node);
        if (it == _content.end() || *it != node) {
            throw std::runtime_error("Attempt to remove unknown node");
        }
        _content.erase(it);
        unsubscribe(node);
        modified(true);
        setState(Node::State::Dirty);
    }

    bool GroupNode::removeIfPresent(std::shared_ptr<Node> const& node) {
        auto it = _content.find(node);
        if (it != _content.end()) {
            if (*it != node) throw std::runtime_error("Attempt to remove unknown node");
            _content.erase(it);
            unsubscribe(node);
            modified(true);
            setState(Node::State::Dirty);
            return true;
        }
        return false;
    }

    std::set<std::shared_ptr<FileNode>, Node::CompareName> GroupNode::files() const {
        std::set<std::shared_ptr<FileNode>, Node::CompareName> files; 
        for (auto const& node : _content) {
            auto const& fileNode = dynamic_pointer_cast<FileNode>(node);
            if (fileNode != nullptr) {
                files.insert(fileNode);
            }  else {
                auto const& cmdNode = dynamic_pointer_cast<CommandNode>(node);
                if (cmdNode != nullptr) {
                    std::vector<std::shared_ptr<GeneratedFileNode>> outputs = cmdNode->detectedOutputs();
                    files.insert(outputs.begin(), outputs.end());
                }
            }
        }
        return files;
    }

    void GroupNode::start(PriorityClass prio) {
        Node::start(prio);
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
        startNodes(requisites, std::move(callback), prio);
    }

    void GroupNode::handleGroupCompletion(Node::State groupState) {
        context()->statistics().registerSelfExecuted(this);
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
        for (auto const& n : _content) {
            hashes.push_back(XXH64_string(n->name().string()));
            auto const& cmd = dynamic_pointer_cast<CommandNode>(n);
            if (cmd != nullptr) {
                std::vector<std::shared_ptr<GeneratedFileNode>> outputs = cmd->detectedOutputs();
                for (auto const& n : outputs) {
                    hashes.push_back(XXH64_string(n->name().string()));
                }
            } else {
                //TODO: add ForEachNode
            }
        }
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    void GroupNode::subscribe(std::shared_ptr<Node> const& node) {
        Node* observable = getObservable(node.get());
        auto it = _observed.find(observable);
        if (it == _observed.end()) {
            _observed[observable] = 1;
            observable->addObserver(this);
        } else {
            uint32_t cnt = it->second;
            if (cnt == 0) throw std::runtime_error("corrupt _observed");
            _observed[observable] = cnt + 1;
        }
    }

    void GroupNode::unsubscribe(std::shared_ptr<Node> const& node) {
        Node* observable = getObservable(node.get());
        auto it = _observed.find(observable);
        if (it == _observed.end()) throw std::runtime_error("illegal unsubscribe request");
        uint32_t cnt = it->second;
        if (cnt == 0) throw std::runtime_error("corrupt _observed");
        cnt -= 1;
        if (cnt == 0) {
            _observed.erase(it);
            observable->removeObserver(this);
        } else {
            _observed[observable] = cnt;
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
        if (streamer->writing()) {
            _contentVec.clear();
            _contentVec.insert(_contentVec.end(), _content.begin(), _content.end());
        }
        streamer->streamVector(_contentVec);
        streamer->stream(_hash);
        if (streamer->writing()) _contentVec.clear();
    }

    void GroupNode::prepareDeserialize() {
        static std::vector<std::shared_ptr<Node>> empty;
        Node::prepareDeserialize();
        for (auto const& node: _content) unsubscribe(node);
        _content.clear();
        _contentVec.clear();
    }

    bool GroupNode::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!Node::restore(context, restored)) return false;
        for (auto const& node : _contentVec) node->restore(context, restored);
        _content.insert(_contentVec.begin(), _contentVec.end());
        _contentVec.clear();
        for (auto const& node : _content) subscribe(node);
        return true;
    }
}

