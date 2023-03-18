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
    DotIgnoreNode::DotIgnoreNode(
        ExecutionContext* context,
        SourceDirectoryNode* directory)
        : Node(context, directory->name() / ".ignore")
        , _directory(directory)
        , _hash(rand())
    {
        setDotIgnoreFiles({
             std::make_shared<SourceFileNode>(context, _directory->name() / ".gitignore"),
             std::make_shared<SourceFileNode>(context, _directory->name() / ".yamignore")
        });
    }

    DotIgnoreNode::~DotIgnoreNode() {
        _dotIgnoreFiles.clear();
    }

    void DotIgnoreNode::clear() {
        setDotIgnoreFiles(std::vector<std::shared_ptr<SourceFileNode>>());
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

    void DotIgnoreNode::setDotIgnoreFiles(std::vector<std::shared_ptr<SourceFileNode>> const& newInputs) {
        if (_dotIgnoreFiles != newInputs) {
            for (auto file : _dotIgnoreFiles) {
                file->removePreParent(this);
                context()->nodes().remove(file);
            }
            _dotIgnoreFiles = newInputs;
            for (auto file : _dotIgnoreFiles) {
                file->addPreParent(this);
                context()->nodes().add(file);
            }
        }
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

    void DotIgnoreNode::startSelf() {
        auto d = Delegate<void>::CreateLambda([this]() { execute(); });
        context()->threadPoolQueue().push(std::move(d));
    }

    void DotIgnoreNode::execute() {
        Node::State newState;
        if (_canceling) {
            newState = Node::State::Canceled;
        } else {
            newState = Node::State::Ok;
            // TODO: parse the dotignore files
        }
        postSelfCompletion(newState);
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
}
