#include "FileNode.h"
#include "FileAspect.h"
#include "ExecutionContext.h"
#include "SourceFileRepository.h"
#include "ExecutionContext.h"
#include "IStreamer.h"

namespace
{
    uint32_t streamableTypeId = 0;
}

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

    std::shared_ptr<FileRepository> FileNode::fileRepository() const {
        return context()->findRepositoryContaining(name());
    }

    std::filesystem::path FileNode::relativePath() const {
        std::shared_ptr<FileRepository> repo = fileRepository();
        if (repo == nullptr) return name();
        return repo->relativePathOf(name());
    }

    std::filesystem::path FileNode::symbolicPath() const {
        std::shared_ptr<FileRepository> repo = fileRepository();
        if (repo == nullptr) return name();
        return repo->symbolicPathOf(name());
    }

    // Note that pendingStartSelf is only called during node execution, i.e. when
    // state() == State::Executing. A node execution only starts when the node was
    // Dirty, i.e. when it is not sure that previously computed hashes are still
    // up-to-date. Therefore always return true.
    bool FileNode::pendingStartSelf() const {
        return true;
    }
     
    std::chrono::time_point<std::chrono::utc_clock> FileNode::retrieveLastWriteTime() const {
        std::error_code ec;
        auto lwt = std::filesystem::last_write_time(name(), ec);

        auto lwutc = decltype(lwt)::clock::to_utc(lwt);
        return lwutc;
    }

    void FileNode::selfExecute(ExecutionResult* result) const{
        result->_newState = Node::State::Failed;
        result->_hashes.clear();
        result->_lastWriteTime = retrieveLastWriteTime();
        bool success = result->_lastWriteTime != std::chrono::utc_clock::time_point::min();
        if (success) {
            result->_newState = Node::State::Ok;
            bool changed = result->_lastWriteTime != _result._lastWriteTime;
            if (changed) {
                std::vector<FileAspect> aspects = context()->findFileAspects(name());
                for (auto const& aspect : aspects) {
                    result->_hashes[aspect.name()] = aspect.hash(name());
                }
            } else {
                result->_hashes = _result._hashes;
            }
        }
    }
       
    void FileNode::commitSelfCompletion(SelfExecutionResult const& result) {
        if (result._newState != Node::State::Ok) {
            _result._lastWriteTime = std::chrono::utc_clock::time_point::min();
            _result._hashes.clear();
            LogRecord error(
                LogRecord::Aspect::Error,
                std::string("Failed to rehash file ").append(name().string()));
            context()->addToLogBook(error);
            return;
        }
        auto const& fileResult = dynamic_cast<ExecutionResult const&>(result);
        bool changed = fileResult._lastWriteTime != _result._lastWriteTime;
        if (changed) {
            _result = fileResult;
            modified(true);
            context()->statistics().registerRehashedFile(this);
            if (_context->logBook()->mustLogAspect(LogRecord::Aspect::FileChanges)) {
                LogRecord progress(
                    LogRecord::Aspect::Progress,
                    std::string("Rehashed file ").append(name().string()));
                context()->addToLogBook(progress);
            }
        }
    }

    void FileNode::selfExecute() {
        auto result = std::make_shared<ExecutionResult>();
        selfExecute(result.get());
        postSelfCompletion(result);
    }

    FileNode::ExecutionResult const& FileNode::result() const {
        return _result;
    }

    std::chrono::time_point<std::chrono::utc_clock> const& FileNode::lastWriteTime() {
        return _result._lastWriteTime;
    }

    XXH64_hash_t FileNode::hashOf(std::string const& aspectName) {
        return _result.hashOf(aspectName);
    }

    void FileNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t FileNode::typeId() const {
        return streamableTypeId;
    }

    void FileNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(_result._lastWriteTime);
        streamer->streamMap(_result._hashes);
    }
}