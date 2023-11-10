#include "FileRepository.h"
#include "DirectoryNode.h"
#include "FileRepositoryWatcher.h"
#include "ExecutionContext.h"
#include "IStreamer.h"

namespace
{
    uint32_t streamableTypeId = 0;
}

namespace YAM
{
    FileRepository::FileRepository()
        : _modified(false)
    {}

    FileRepository::FileRepository(
        std::string const& repoName,
        std::filesystem::path const& directory,
        ExecutionContext* context)
        : _name(repoName)
        , _directory(directory)
        , _context(context)
        , _watcher(std::make_shared<FileRepositoryWatcher>(directory, context))
        , _directoryNode(std::make_shared<DirectoryNode>(context, directory, nullptr))
        , _modified(true)
    {
        _context->nodes().add(_directoryNode);
        _directoryNode->addPrerequisitesToContext();
    }

    std::string const& FileRepository::name() const {
        return _name;
    }

    std::filesystem::path const& FileRepository::directory() const {
        return _directory;
    }

    bool FileRepository::lexicallyContains(std::filesystem::path const& absPath) const {
        const std::size_t result = absPath.string().find(_directory.string());
        return result == 0;
    }

    std::filesystem::path FileRepository::relativePathOf(std::filesystem::path const& absPath) const {
        std::filesystem::path relPath;
        const std::string absStr = absPath.string();
        const std::string dirStr = (_directory / "").string();
        const std::size_t result = absStr.find(dirStr);
        if (result == 0) {
            relPath = absStr.substr(result + dirStr.length(), absStr.length() - dirStr.length());
        }
        return relPath;
    }

    std::filesystem::path FileRepository::symbolicPathOf(std::filesystem::path const& absPath) const {
        auto relPath = relativePathOf(absPath);
        if (relPath.empty()) return relPath;
        return std::filesystem::path(_name) / relPath;
    }

    void FileRepository::modified(bool newValue) {
        _modified = newValue;
    }

    bool FileRepository::modified() const {
        return _modified;
    }

    std::shared_ptr<DirectoryNode> FileRepository::directoryNode() const {
        return _directoryNode; 
    }

    void FileRepository::consumeChanges() {
        _watcher->consumeChanges();
    }

    bool FileRepository::hasChanged(std::filesystem::path const& path) {
        return _watcher->hasChanged(path);
    }

    void FileRepository::clear() {
        _context->nodes().remove(_directoryNode);
        _directoryNode->clear();
        _directoryNode->setState(Node::State::Dirty);
        modified(true);
    }

    void FileRepository::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t FileRepository::typeId() const {
        return streamableTypeId;
    }

    void FileRepository::stream(IStreamer* streamer) {
        streamer->stream(_name);
        streamer->stream(_directory);
        streamer->stream(_directoryNode);
    }

    void FileRepository::prepareDeserialize() {
    }

    void FileRepository::restore(void* context) {
        _context = reinterpret_cast<ExecutionContext*>(context);
        if (_watcher == nullptr || _watcher->directory() != _directory) {
            _watcher = std::make_shared<FileRepositoryWatcher>(_directory, _context);
        }
    }
}