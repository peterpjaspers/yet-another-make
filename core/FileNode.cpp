#include "FileNode.h"
#include "FileAspect.h"
#include "ExecutionContext.h"
#include "FileRepositoryNode.h"
#include "ExecutionContext.h"
#include "IStreamer.h"
#include "ILogBook.h"

namespace
{
    uint32_t streamableTypeId = 0;
}

namespace YAM
{
    FileNode::FileNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
    {}
     
    std::chrono::time_point<std::chrono::utc_clock> FileNode::retrieveLastWriteTime() const {
        std::error_code ec;
        auto lwt = std::filesystem::last_write_time(absolutePath(), ec);
        auto lwutc = decltype(lwt)::clock::to_utc(lwt);
        return lwutc;
    }

    void FileNode::start() {
        Node::start();
        context()->statistics().registerSelfExecuted(this);
        auto d = Delegate<void>::CreateLambda([this]() {execute(); });
        context()->threadPoolQueue().push(std::move(d));
    }

    void FileNode::execute() {
        auto newState = Node::State::Ok;
        std::map<std::string, XXH64_hash_t> newHashes;
        auto lwt = _lastWriteTime;
        auto newLastWriteTime = retrieveLastWriteTime();
        if (newLastWriteTime != _lastWriteTime) {
            std::vector<FileAspect> aspects = context()->findFileAspects(name());
            for (auto const& aspect : aspects) {
                newHashes[aspect.name()] = aspect.hash(absolutePath());
            }
            auto lastWriteTime = retrieveLastWriteTime();
            if (lastWriteTime != newLastWriteTime) {
                // file was modified while being hashed.
                newState = Node::State::Failed;
                for (auto const& pair : newHashes) {
                    newHashes[pair.first] = rand();
                }
            }
        }
        auto d = Delegate<void>::CreateLambda(
            [this, newState, newLastWriteTime, newHashes]() {
                finish(newState, newLastWriteTime, newHashes);
            });
        context()->mainThreadQueue().push(std::move(d));
    }
       
    void FileNode::finish(
        Node::State newState,
        std::chrono::time_point<std::chrono::utc_clock> const& newLastWriteTime,
        std::map<std::string, XXH64_hash_t> const& newHashes
    ) {
        if (newState == Node::State::Ok) {
            if (newLastWriteTime != _lastWriteTime) {
                _lastWriteTime = newLastWriteTime;
                bool changedContent = _hashes != newHashes;
                _hashes = newHashes;
                modified(true);
                if (changedContent) {
                    std::stringstream ss;
                    ss << className() << " " << name().string() << " has changed file content.";
                    LogRecord change(LogRecord::FileChanges, ss.str());
                    context()->logBook()->add(change);
                }
                context()->statistics().registerRehashedFile(this);
            }
        } else {
            std::stringstream ss;
            ss << "File " << absolutePath() << " was modified while being hashed." << std::endl;
            ss << "Restart the build to get correct output." << std::endl;
            LogRecord error(LogRecord::Error, ss.str());
            context()->logBook()->add(error);
        }
        Node::notifyCompletion(newState);
    }

    XXH64_hash_t FileNode::hashOf(std::string const& aspectName) {
        if (state() == Node::State::Deleted) return rand();
        auto it = _hashes.find(aspectName);
        if (it == _hashes.end()) throw std::runtime_error("no such aspect");
        return it->second;
    }

    void FileNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t FileNode::typeId() const {
        return streamableTypeId;
    }

    void FileNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(_lastWriteTime);
        streamer->streamMap(_hashes);
    }
}