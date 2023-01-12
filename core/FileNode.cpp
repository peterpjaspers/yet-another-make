#include "FileNode.h"
#include "FileAspect.h"
#include "ExecutionContext.h"

namespace YAM
{
    FileNode::FileNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
    {}

    bool FileNode::supportsPrerequisites() const {
        return false;
    }

    void FileNode::getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const {
    }

    bool FileNode::supportsOutputs() const {
        return false;
    }

    void FileNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
    }

    bool FileNode::supportsInputs() const {
        return false;
    }

    void FileNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
    }

    XXH64_hash_t FileNode::hashOf(std::string const& aspectName) {
        if (!_hashes.contains(aspectName)) throw std::runtime_error("no such aspect");
        return _hashes[aspectName];
    }

    // Note that pendingStartSelf is only called during node execution, i.e. when
    // state() == State::Executing. A node execution only starts when the node was
    // Dirty, i.e. when it is not sure that previously computed hashes are still
    // up-to-date. Therefore always return true.
    bool FileNode::pendingStartSelf() const {
        return true;
    }

    void FileNode::startSelf() {
        auto d = Delegate<void>::CreateLambda([this]() { execute(); });
        _context->threadPoolQueue().push(std::move(d));
    }

    // Sync _lastWriteTime with current file state. 
    // Return whether it was changed since previous update.
    bool FileNode::updateLastWriteTime() {
        std::error_code ec;
        auto lwt = std::filesystem::last_write_time(name(), ec);
        auto lwutc = decltype(lwt)::clock::to_utc(lwt);
        if (lwutc != _lastWriteTime) {
            _lastWriteTime = lwutc;
            return true;
        }
        return false;
    }

    void FileNode::rehashAll(bool doUpdateLastWriteTime) {
        if (doUpdateLastWriteTime) updateLastWriteTime();
        std::vector<FileAspect> aspects = context()->findFileAspects(name());
        for (auto const& aspect : aspects) {
            _hashes[aspect.name()] = aspect.hash(name());
        }
    }

    void FileNode::rehashAll() {
        rehashAll(true);
    }

    void FileNode::execute() {
        if (updateLastWriteTime()) {
            rehashAll(false);
        }
        postSelfCompletion(Node::State::Ok);
    }
}