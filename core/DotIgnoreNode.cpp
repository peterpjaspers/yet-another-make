#include "DotIgnoreNode.h"
#include "ExecutionContext.h"
#include "SourceFileNode.h"
#include "SourceDirectoryNode.h"
#include "FileSystem.h"
#include "FileAspect.h"
#include "SourceFileRepository.h"
#include "IStreamer.h"

namespace
{
    using namespace YAM;

    uint32_t streamableTypeId = 0;

    void setDirtyRecursively(Node* node) {
        node->setState(Node::State::Dirty);
        SourceDirectoryNode* dir = dynamic_cast<SourceDirectoryNode*>(node);
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

    DotIgnoreNode::DotIgnoreNode() : Node() {}

    DotIgnoreNode::DotIgnoreNode(
        ExecutionContext* context,
        std::filesystem::path const& name,
        SourceDirectoryNode* directory)
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
            file->addPreParent(this);
        }
    }

    void DotIgnoreNode::clear() {
        for (auto file : _dotIgnoreFiles) {
            context()->nodes().remove(file);
            file->removePreParent(this);
        }
        _dotIgnoreFiles.clear();
    }

    void DotIgnoreNode::setState(State newState) {
        if (state() != newState) {
            Node::setState(newState);
            if (state() == Node::State::Dirty) {
                // Given de gitignore precedence rules a change in ignore files
                // in some directory D affects all sub-directories of D.
                setDirtyRecursively(_directory);
            }
        }
    }

    bool DotIgnoreNode::supportsPrerequisites() const { return true; }
    void DotIgnoreNode::getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const {
        prerequisites.insert(prerequisites.end(), _dotIgnoreFiles.begin(), _dotIgnoreFiles.end());
    }

    bool DotIgnoreNode::supportsInputs() const { return true; }
    void DotIgnoreNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
        inputs.insert(inputs.end(), _dotIgnoreFiles.begin(), _dotIgnoreFiles.end());
    }

    XXH64_hash_t DotIgnoreNode::hash() const {
        return _hash;
    }

    bool DotIgnoreNode::ignore(std::filesystem::path const& path) const {
        auto fileName = path.filename();
        bool ignore = fileName == ".gitignore" || fileName == ".yamignore";
        if (!ignore) {
            auto repo = context()->findRepositoryContaining(path);
            ignore = repo == nullptr;
            if (repo != nullptr) {
                auto srcRepo = dynamic_pointer_cast<SourceFileRepository>(repo);
                if (srcRepo != nullptr) {
                    ignore = srcRepo->excludePatterns().matches(path.string());
                }
            }
        }
        return ignore;
    }

    void DotIgnoreNode::directory(SourceDirectoryNode* directory) {
        _directory = directory;
    }

    bool DotIgnoreNode::pendingStartSelf() const {
        bool pending = _hash != computeHash();
        return pending;
    }

    XXH64_hash_t DotIgnoreNode::computeHash() const {
        std::vector<XXH64_hash_t> hashes;
        for (auto const& node : _dotIgnoreFiles) {
            hashes.push_back(node->hashOf(FileAspect::entireFileAspect().name()));
        }
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    void DotIgnoreNode::selfExecute() {
        auto result = std::make_shared<SelfExecutionResult>();
        if (_canceling) {
            result->_newState = Node::State::Canceled;
        } else {
            result->_newState = Node::State::Ok;
            // TODO: parse the dotignore files
        }
        modified(true);
        //postSelfCompletion(newState);
        postSelfCompletion(result);
    }

    void DotIgnoreNode::commitSelfCompletion(SelfExecutionResult const* result) {
        // TODO
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
        for (auto file : _dotIgnoreFiles) {
            file->removePreParent(this);
        }
        _dotIgnoreFiles.clear();
    }

    void DotIgnoreNode::restore(void* context) {
        Node::restore(context);
        for (auto file : _dotIgnoreFiles) {
            file->addPreParent(this);
        }
    }
}
