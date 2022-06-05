#include "FileNode.h"
#include "ExecutionContext.h"
#include <cstdio>

namespace YAM
{
    FileNode::FileNode(ExecutionContext* context, std::filesystem::path name)
        : Node(context, name)
    {}

    bool FileNode::supportsPrerequisites() const {
        return false;
    }

    void FileNode::appendPrerequisites(std::vector<Node*>& prerequisites) const {
    }

    bool FileNode::supportsOutputs() const {
        return false;
    }

    void FileNode::appendOutputs(std::vector<Node*>& outputs) const {
    }

    bool FileNode::supportsInputs() const {
        return false;
    }

    void FileNode::appendInputs(std::vector<Node*>& inputs) const {
    }

    // Note that pendingStartSelf is (intended to be) only called during node execution.
    // A node execution only starts when the node was dirty, i.e. when the last computed 
    // file hash may have changed. Therefore always return true.
    bool FileNode::pendingStartSelf() const {
        return true;
    }

    void FileNode::startSelf() {
        auto d = Delegate<void>::CreateLambda([this]() { execute(); });
        _context->threadPoolQueue().push(std::move(d));
    }

    std::chrono::time_point<std::chrono::utc_clock> FileNode::lastWriteTime() const {
        return _lastWriteTime;
    }

    XXH64_hash_t FileNode::computeExecutionHash() const {
        return XXH64_file(name().string().c_str());
    }

    void FileNode::rehash() {
        std::error_code error;
        auto lwtime = std::filesystem::last_write_time(name(), error);
        if (!error) {
            auto lwutc = decltype(lwtime)::clock::to_utc(lwtime);
            if (lwutc != _lastWriteTime) {
                _lastWriteTime = lwutc;
                _executionHash = computeExecutionHash();
            }
        } else {
            _lastWriteTime = std::chrono::time_point<std::chrono::utc_clock>::min();
            _executionHash = rand();
        }
    }

    void FileNode::execute() {
        rehash();
        postCompletion(Node::State::Ok);
    }
}