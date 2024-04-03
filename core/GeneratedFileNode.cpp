#include "GeneratedFileNode.h"
#include "ExecutionContext.h"
#include "CommandNode.h"
#include "IStreamer.h"

namespace
{
    uint32_t streamableTypeId = 0;
}

namespace YAM
{
    GeneratedFileNode::GeneratedFileNode(
        ExecutionContext* context,
        std::filesystem::path const& name,
        std::shared_ptr<CommandNode> const& producer)
        : FileNode(context, name)
        , _producer(producer) 
    {}

    void GeneratedFileNode::cleanup() {
        deleteFile(false, true);
        _producer = nullptr;
    }

    std::shared_ptr<CommandNode> GeneratedFileNode::producer() const {
        return _producer;
    }

    bool GeneratedFileNode::deleteFile(bool setDirty, bool logDeletion) {
        std::error_code ec;
        auto absPath = absolutePath();
        bool deleted = !std::filesystem::exists(absPath);
        if (!deleted) {
            deleted = std::filesystem::remove(absPath, ec);
            if (deleted && setDirty) {
                setState(Node::State::Dirty);
            }
            if (logDeletion) {
                if (deleted) {
                    LogRecord progress(LogRecord::Aspect::Progress, "Deleted " + absPath.string());
                    context()->logBook()->add(progress);
                } else {
                    LogRecord error(LogRecord::Aspect::Error, "Failed to delete " + absPath.string());
                    context()->logBook()->add(error);
                }
            }
        }
        return deleted;
    }

    void GeneratedFileNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t GeneratedFileNode::typeId() const {
        return streamableTypeId;
    }

    void GeneratedFileNode::stream(IStreamer* streamer) {
        FileNode::stream(streamer); 
        streamer->stream(_producer);
    }
}