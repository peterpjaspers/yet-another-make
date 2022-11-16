#include "FileNode.h"
#include "FileAspect.h"
#include "ExecutionContext.h"
#include <cstdio>

namespace
{
    using namespace YAM;

    XXH64_hash_t HashFile(std::filesystem::path const& fileName) {
        return XXH64_file(fileName.string().c_str());
    }
}

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

    bool FileNode::addAspect(std::string const& aspectName)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        bool mustAdd = !_hashes.contains(aspectName);
        if (mustAdd) {
            _hashes.insert({ aspectName, rand() });
            {
                auto& allHashers = _context->aspectHashers();
                std::lock_guard<std::mutex> allHashersLock(allHashers.mutex());
                auto& hasher = allHashers.find(aspectName);
                _hashers.insert({ aspectName, hasher });
            }
        }
        return mustAdd;
    }

    XXH64_hash_t FileNode::hashOf(std::string const& aspectName) {
        std::lock_guard<std::mutex> lock(_mutex);
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

    XXH64_hash_t FileNode::rehash(std::string const& aspectName) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _hashers.find(aspectName);
        if (it == _hashers.end()) throw std::runtime_error("unknown aspect");
        auto const & hasher = it->second;
        auto hash = hasher.hash(name());
        _hashes[aspectName] = hash;
        return hash;
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
        for (auto& entry : _hashers) {
            auto& aspectName = entry.first;
            auto& hasher = entry.second;
            _hashes[aspectName] = hasher.hash(name());
        }
    }

    void FileNode::rehashAll() {
        // Note: no lock needed because this function is intended to only
        // be called on output files by command node that re-computed
        // these outputs. This happens during command node execution.
        // The command that produced/updated the outputfile is a prerequiste
        // of a commands that uses that output file as input file. Hence this
        // command will not access the output file until all its prerequisite
        // has completed execution.
        //
        rehashAll(true);
    }

    void FileNode::execute() {
        if (updateLastWriteTime()) {
            rehashAll(false);
        }
        postCompletion(Node::State::Ok);
    }
}