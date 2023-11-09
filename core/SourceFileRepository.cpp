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
        : _modified(false)
    {}

    SourceFileRepository::SourceFileRepository(
        std::string const& repoName,
        std::filesystem::path const& directory,
        ExecutionContext* context)
        : _name(repoName)
        , _directory(directory)
        , _context(context)
        , _watcher(std::make_shared<FileRepositoryWatcher>(directory, context))
        , _directoryNode(std::make_shared<SourceDirectoryNode>(context, directory, nullptr))
        , _modified(true)
    {
        _context->nodes().add(_directoryNode);
        _directoryNode->addPrerequisitesToContext();
    }

    std::string const& SourceFileRepository::name() const {
        return _name;
    }

    std::filesystem::path const& SourceFileRepository::directory() const {
        return _directory;
    }

    bool SourceFileRepository::lexicallyContains(std::filesystem::path const& absPath) const {
        const std::size_t result = absPath.string().find(_directory.string());
        return result == 0;
    }

    std::filesystem::path SourceFileRepository::relativePathOf(std::filesystem::path const& absPath) const {
        std::filesystem::path relPath;
        const std::string absStr = absPath.string();
        const std::string dirStr = (_directory / "").string();
        const std::size_t result = absStr.find(dirStr);
        if (result == 0) {
            relPath = absStr.substr(result + dirStr.length(), absStr.length() - dirStr.length());
        }
        return relPath;
    }

    std::filesystem::path SourceFileRepository::symbolicPathOf(std::filesystem::path const& absPath) const {
        auto relPath = relativePathOf(absPath);
        if (relPath.empty()) return relPath;
        return std::filesystem::path(_name) / relPath;
    }

    void SourceFileRepository::modified(bool newValue) {
        _modified = newValue;
    }

    bool SourceFileRepository::modified() const {
        return _modified;
    }

    std::shared_ptr<SourceDirectoryNode> SourceFileRepository::directoryNode() const {
        return _directoryNode; 
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
        streamer->stream(_name);
        streamer->stream(_directory);
        streamer->stream(_directoryNode);
    }

    void SourceFileRepository::restore(void* context) {
        if (_context == nullptr) {
            _context = reinterpret_cast<ExecutionContext*>(context);
            if (_directoryNode != nullptr) {
                _watcher = std::make_shared<FileRepositoryWatcher>(_directory, _context);
            }
        }
    }
}