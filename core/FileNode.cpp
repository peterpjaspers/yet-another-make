#include "FileNode.h"
#include "ExecutionContext.h"
#include <cstdio>

namespace
{
    using namespace YAM;

    XXH64_hash_t HashFile(std::filesystem::path const& fileName) {
        return XXH64_file(fileName.string().c_str());
    }

    FileNode::AspectHasher entireFileHasher {
        std::string("entireFile"), 
        Delegate<XXH64_hash_t, std::filesystem::path const&>::CreateStatic(HashFile) 
    };
}

namespace YAM
{
    FileNode::FileNode(ExecutionContext* context, std::filesystem::path name)
        : Node(context, name)
    {
        // by default hash entire file content
        setAspectHashers({ entireFileHasher });
    }

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

    // Hashers are set when the node is created, when the hasher configuration has
    // changed or after restore of the node from buildstate. Note that hashers cannot
    // be saved to build state because functions cannot be serialized.  
    // On restore after buildstate the node _hashes may still be up-to-date.
    // Therefore make sure to only invalidate hashes when different aspects
    // are requested. 
    void FileNode::setAspectHashers(std::vector<AspectHasher> const& newHashers) {
        _hashers = newHashers;
        bool changedAspects = _hashers.size() != _hashes.size();
        if (!changedAspects) {
            for (auto& hasher : _hashers) {
                if (!_hashes.contains(hasher.name)) {
                    changedAspects = true;
                    break;
                }
            }
        }
        if (changedAspects) {
            // changing last write time causes re-computation of hashes.
            _lastWriteTime = std::chrono::time_point<std::chrono::utc_clock>::min();
            for (auto& entry : _hashers) {
                _hashes[entry.name] = rand();
            }
        }
        // Set state dirty to re-execute node. Note that re-execution will only rehash
        // when file last write time has changed.
        setState(State::Dirty);
    }

    XXH64_hash_t FileNode::hashOf(std::string const& aspectName) {
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

    void FileNode::rehash() {
        std::error_code error;
        auto lwtime = std::filesystem::last_write_time(name(), error);
        if (!error) {
            auto lwutc = decltype(lwtime)::clock::to_utc(lwtime);
            if (lwutc != _lastWriteTime) {
                _lastWriteTime = lwutc;
                for (auto& entry : _hashers) {
                    _hashes[entry.name] = entry.hashFunction.Execute(name());
                }
            }
        } else {
            for (auto& entry : _hashers) {
                _hashes[entry.name] = rand();
            }
        }
    }

    void FileNode::execute() {
        rehash();
        postCompletion(Node::State::Ok);
    }
}