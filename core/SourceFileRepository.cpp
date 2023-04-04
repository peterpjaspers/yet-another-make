#include "SourceFileRepository.h"
#include "SourceDirectoryNode.h"
#include "FileRepositoryWatcher.h"
#include "ExecutionContext.h"
#include "IStreamer.h"

namespace
{
    uint32_t streamableTypeId = 0;
}

namespace YAM
{
    SourceFileRepository::SourceFileRepository()
        : FileRepository()
        , _context(nullptr)
    {}

    SourceFileRepository::SourceFileRepository(
        std::string const& repoName,
        std::filesystem::path const& directory,
        RegexSet const& excludes,
        ExecutionContext* context)
        : FileRepository(repoName, directory)
        , _watcher(std::make_shared<FileRepositoryWatcher>(directory, context))
        , _excludePatterns(excludes)
        , _context(context)
        , _directoryNode(std::make_shared<SourceDirectoryNode>(context, directory))
    {
        _context->nodes().add(_directoryNode);
    }

    std::shared_ptr<SourceDirectoryNode> SourceFileRepository::directoryNode() const {
        return _directoryNode; 
    }

    RegexSet const& SourceFileRepository::excludePatterns() const {
        return _excludePatterns;
    }

    void SourceFileRepository::consumeChanges() {
        _watcher->consumeChanges();
    }

    bool SourceFileRepository::hasChanged(std::filesystem::path const& path) {
        return _watcher->hasChanged(path);
    }

    void SourceFileRepository::clear() {
        _context->nodes().remove(_directoryNode);
        _directoryNode->clear();
        _directoryNode->setState(Node::State::Dirty);
        modified(true);
    }

    void SourceFileRepository::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t SourceFileRepository::typeId() const {
        return streamableTypeId;
    }

    void SourceFileRepository::stream(IStreamer* streamer) {
        FileRepository::stream(streamer);
        _excludePatterns.stream(streamer);
        streamer->stream(_directoryNode);
    }

    void SourceFileRepository::restore(void* context) {
        FileRepository::restore(context);
        if (_context == nullptr) {
            _context = reinterpret_cast<ExecutionContext*>(context);
            _watcher = std::make_shared<FileRepositoryWatcher>(_directory, _context);
        }
    }
}