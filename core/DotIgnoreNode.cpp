#include "DotIgnoreNode.h"
#include "ExecutionContext.h"
#include "SourceFileNode.h"
#include "DirectoryNode.h"
#include "FileSystem.h"
#include "FileAspect.h"
#include "FileRepository.h"
#include "IStreamer.h"

namespace
{
    using namespace YAM;

    uint32_t streamableTypeId = 0;

    void setDirtyRecursively(Node* node) {
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
            context()->nodes().remove(file);
            file->removeObserver(this);
        }
        _dotIgnoreFiles.clear();
    }

    void DotIgnoreNode::setState(State newState) {
        if (state() != newState) {
            Node::setState(newState);
            if (newState == Node::State::Dirty) {
                // Given de gitignore precedence rules a change in ignore files
                // in some directory D affects all sub-directories of D.
                setDirtyRecursively(_directory);
            }
        }
    }

    bool DotIgnoreNode::ignore(std::filesystem::path const& path) const {
        return false;
    }

    XXH64_hash_t DotIgnoreNode::computeHash() const {
        std::vector<XXH64_hash_t> hashes;
        for (auto const& node : _dotIgnoreFiles) {
            hashes.push_back(node->hashOf(FileAspect::entireFileAspect().name()));
        }
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    void DotIgnoreNode::start() {
        Node::start();
        std::vector<Node*> requisites;
        for (auto const& n : _dotIgnoreFiles) requisites.push_back(n.get());
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State s) { handleRequisiteCompletion(s); }
        );
        Node::startNodes(requisites, std::move(callback));
    }

    void DotIgnoreNode::handleRequisiteCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            Node::notifyCompletion(state);
        } else if (canceling()) {
            Node::notifyCompletion(Node::State::Canceled);
        } else if (_hash != computeHash()) {
            context()->statistics().registerSelfExecuted(this);
            auto d = Delegate<void>::CreateLambda(
                [this]() { parseDotIgnoreFiles(); }
            );
            context()->threadPoolQueue().push(std::move(d));
        } else {
            Node::notifyCompletion(state);
        }
    }

    void DotIgnoreNode::parseDotIgnoreFiles() {
        if (canceling()) {
            postCompletion(Node::State::Canceled);
        } else {
            // TODO: parse the dotignore files
            //modified(true);
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
    }

    void DotIgnoreNode::prepareDeserialize() {
        Node::prepareDeserialize();
        for (auto file : _dotIgnoreFiles) file->removeObserver(this);
        _dotIgnoreFiles.clear();
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
