#include "DotIgnoreNode.h"
#include "DotIgnoreParser.h"
#include "ExecutionContext.h"
#include "SourceFileNode.h"
#include "DirectoryNode.h"
#include "FileSystem.h"
#include "FileAspect.h"
#include "RepositoriesNode.h"
#include "FileRepositoryNode.h"
#include "IStreamer.h"

namespace
{
    using namespace YAM;

    uint32_t streamableTypeId = 0;

    void setDirtyRecursively(Node* node) {
        if (node->state() == Node::State::Deleted) return;
        node->setState(Node::State::Dirty);
        DirectoryNode* dir = dynamic_cast<DirectoryNode*>(node);
        if (dir != nullptr) {
            auto const& content = dir->getContent();
            for (auto const& pair : content) {
                Node* node = pair.second.get();
                setDirtyRecursively(node);
            }
        }
    }

    void parseRules(
        std::filesystem::path const& ignoreFile, 
        std::vector<DotIgnoreRule> & rules
    ) {
        DotIgnoreParser parser(ignoreFile);
        rules.insert(
            rules.end(),
            parser.rules().begin(),
            parser.rules().end());
    }
}

namespace YAM
{
    DotIgnoreNode::DotIgnoreNode() : Node(), _directory(nullptr) {}

    DotIgnoreNode::DotIgnoreNode(
        ExecutionContext* context,
        std::filesystem::path const& name,
        DirectoryNode* directory)
        : Node(context, name)
        , _directory(directory)
        , _hash(rand())
    {
        _dotIgnoreFiles.push_back(std::make_shared<SourceFileNode>(context, _directory->name() / ".gitignore"));
        _dotIgnoreFiles.push_back(std::make_shared<SourceFileNode>(context, _directory->name() / ".yamignore"));
    }

    void DotIgnoreNode::addPrerequisitesToContext() {
        for (auto file : _dotIgnoreFiles) {
            context()->nodes().add(file);
            file->addObserver(this);
        }
    }

    void DotIgnoreNode::directory(DirectoryNode* directory) {
        _directory = directory;
    }

    void DotIgnoreNode::clear() {
        for (auto file : _dotIgnoreFiles) {
            file->removeObserver(this);
            context()->nodes().remove(file);
        }
        _dotIgnoreFiles.clear();
        _rules.clear();
    }

    void DotIgnoreNode::setState(State newState) {
        if (state() != newState) {
            Node::setState(newState);
            if (newState == Node::State::Dirty) {
                // Given de gitignore precedence rules a change in ignore file
                // in some directory D affects all sub-directories of D.
                setDirtyRecursively(_directory);
            }
        }
    }

    bool DotIgnoreNode::ignore(std::filesystem::path const& path) const {
        for (auto const& file : _dotIgnoreFiles) {
            if (file->name().filename() == path) return true;
        }
        for (auto it = _rules.rbegin(); it != _rules.rend(); ++it) {
            auto const& rule = *it;
            if (rule.match(path)) {
                return !rule.negate();
            }
        }
        auto parentDir = _directory->parent();
        if (parentDir != nullptr) {
            auto dirName = _directory->name().filename();
            return parentDir->dotIgnoreNode()->ignore(dirName / path);
        }
        return false;
    }

    XXH64_hash_t DotIgnoreNode::computeHash() const {
        std::vector<XXH64_hash_t> hashes;
        if (_directory->parent() != nullptr) {
            hashes.push_back(_directory->parent()->dotIgnoreNode()->hash());
        }
        for (auto const& node : _dotIgnoreFiles) {
            hashes.push_back(node->hashOf(FileAspect::entireFileAspect().name()));
        }
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    void DotIgnoreNode::start(PriorityClass prio) {
        Node::start(prio);
        std::vector<Node*> requisites;
        if (_directory->parent() != nullptr) {
            auto parentDotIgnoreNode = _directory->parent()->dotIgnoreNode();
            parentDotIgnoreNode->addObserver(this);
            requisites.push_back(parentDotIgnoreNode.get());
        }
        for (auto const& n : _dotIgnoreFiles) requisites.push_back(n.get());
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State s) { handleRequisiteCompletion(s); }
        );
        startNodes(requisites, std::move(callback), prio);
    }

    void DotIgnoreNode::handleRequisiteCompletion(Node::State state) {
        if (_directory->parent() != nullptr) {
            auto parentDotIgnoreNode = _directory->parent()->dotIgnoreNode();
            parentDotIgnoreNode->removeObserver(this);
        }
        if (state != Node::State::Ok) {
            Node::notifyCompletion(state);
        } else if (canceling()) {
            Node::notifyCompletion(Node::State::Canceled);
        } else if (_hash != computeHash()) {
            context()->statistics().registerSelfExecuted(this);
            auto d = Delegate<void>::CreateLambda(
                [this]() { parseDotIgnoreFiles(); }
            );
            context()->threadPoolQueue().push(std::move(d), PriorityClass::High);
        } else {
            Node::notifyCompletion(state);
        }
    }

    void DotIgnoreNode::parseDotIgnoreFiles() {
        if (canceling()) {
            postCompletion(Node::State::Canceled);
        } else {
            _rules.clear();
            for (auto const& n : _dotIgnoreFiles) {
                parseRules(n->absolutePath(), _rules);
            }
            _hash = computeHash();
            modified(true);
            postCompletion(Node::State::Ok);
        }
    }

    void DotIgnoreNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t DotIgnoreNode::typeId() const {
        return streamableTypeId;
    }

    void DotIgnoreNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->streamVector(_dotIgnoreFiles);
        streamer->stream(_hash);
        DotIgnoreRule::streamVector(streamer, _rules);
    }

    void DotIgnoreNode::prepareDeserialize() {
        Node::prepareDeserialize();
        for (auto file : _dotIgnoreFiles) file->removeObserver(this);
        _dotIgnoreFiles.clear();
        _rules.clear();
    }

    bool DotIgnoreNode::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!Node::restore(context, restored)) return false;
        for (auto file : _dotIgnoreFiles) {
            file->restore(context, restored);
            file->addObserver(this);
        }
        return true;
    }
}
