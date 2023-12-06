#include "FileRepository.h"
#include "DirectoryNode.h"
#include "FileRepositoryWatcher.h"
#include "ExecutionContext.h"
#include "IStreamer.h"

namespace
{
    uint32_t streamableTypeId = 0;

    bool equalPaths(std::filesystem::path const& p1, std::filesystem::path const& p2) {
        auto p1it = p1.begin();
        auto p2it = p2.begin();
        bool equal = true;
        for (;
            p1it != p1.end() && p2it != p2.end() && equal;
            p1it++, p2it++
        ) {
            equal = *p1it == *p2it;
        }
        return equal;
    }
}

namespace YAM
{
    FileRepository::FileRepository()
        : _context(nullptr)
        , _modified(false)
    {}

    FileRepository::FileRepository(
        std::string const& repoName,
        std::filesystem::path const& directory,
        ExecutionContext* context)
        : _name(repoName)
        , _directory(directory)
        , _symbolicDirectory(repoNameToSymbolicPath(repoName))
        , _context(context)
        , _watcher(std::make_shared<FileRepositoryWatcher>(this, context))
        , _directoryNode(std::make_shared<DirectoryNode>(context, _symbolicDirectory, nullptr))
        , _modified(true)
    {
        _context->nodes().add(_directoryNode);
        _directoryNode->addPrerequisitesToContext();
    }

    FileRepository::~FileRepository() {
    }

    std::string const& FileRepository::name() const {
        return _name;
    }

    std::filesystem::path const& FileRepository::directory() const {
        return _directory;
    }

    bool FileRepository::isSymbolicPath(std::filesystem::path const& path) {
        auto it = path.begin();
        if (it == path.end()) return false;
        std::string repoComponent = (*it).string();
        std::size_t n = repoComponent.length();
        if (n <= 2) return false;
        if (repoComponent[0] != '<') return false;
        if (repoComponent[n - 1] != '>') return false;
        return true;
    }

    std::string FileRepository::repoNameFromPath(std::filesystem::path const& path) {
        static std::string empty;
        auto it = path.begin();
        if (it == path.end()) return empty;
        std::string repoComponent = (*it).string();
        std::size_t n = repoComponent.length();
        if (n <= 2) return empty;
        if (repoComponent[0] != '<') return empty;
        if (repoComponent[n - 1] != '>') return empty;
        return repoComponent.substr(1, n - 2);
    }

    std::filesystem::path FileRepository::repoNameToSymbolicPath(std::string const& repoName) {
        std::filesystem::path p("<" + repoName + ">");
        return p;
    }

    bool FileRepository::lexicallyContains(std::filesystem::path const& path) const {
        bool contains = true;
        if (path.is_absolute()) {
            auto pit = path.begin();
            auto rit = _directory.begin();
            for (; 
                pit != path.end() && rit != _directory.end() && contains;
                pit++, rit++
            ) {
                contains = *pit == *rit;
            }
        } else {
            auto it = path.begin();
            contains = (it != path.end() && *it == _directoryNode->name());
        }
        return contains;
    }

    std::filesystem::path FileRepository::relativePathOf(std::filesystem::path const& absPath) const {
        if (!absPath.is_absolute()) throw std::runtime_error("not an absolute path");
        std::filesystem::path relPath;
        auto pit = absPath.begin();
        auto rit = _directory.begin();
        bool contains = true;
        for (;
            pit != absPath.end() && rit != _directory.end() && contains;
            pit++, rit++
        ) {
            contains = *pit == *rit;
        }
        if (contains) {
            for (; pit != absPath.end(); pit++) {
                relPath /= *pit;
            }
        }
        return relPath;
    }

    std::filesystem::path FileRepository::symbolicPathOf(std::filesystem::path const& absPath) const {
        if (!absPath.is_absolute()) {
            throw std::runtime_error("not an absolute path");
        }
        bool contains = true;
        auto pit = absPath.begin();
        auto rit = _directory.begin();
        for (;
            pit != absPath.end() && rit != _directory.end() && contains;
            pit++, rit++
        ) {
            contains = *pit == *rit;
        }
        std::filesystem::path symPath;
        if (contains) {
            symPath /= repoNameToSymbolicPath(name());
            for (; pit != absPath.end(); pit++) {
                symPath /= *pit;
            }
        }
        return symPath;
    }

    std::filesystem::path FileRepository::absolutePathOf(std::filesystem::path const& symbolicPath) const {
        std::filesystem::path absPath;
        std::string symRepoName = repoNameFromPath(symbolicPath);
        if (symRepoName != _name) return absPath;
        absPath = _directory;
        auto it = symbolicPath.begin();
        it++; // skip repo name component
        for (; it != symbolicPath.end(); it++) {
            absPath /= *it;
        }
        return absPath;
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
            _watcher = std::make_shared<FileRepositoryWatcher>(this, _context);
        }
        _symbolicDirectory = repoNameToSymbolicPath(_name);
    }
}