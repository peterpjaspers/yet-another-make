#include "FileRepository.h"
#include "DirectoryNode.h"
#include "SourceFileNode.h"
#include "FileRepositoryWatcher.h"
#include "ExecutionContext.h"
#include "FileExecSpecsNode.h"
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
        ExecutionContext* context,
        bool tracked)
        : _name(repoName)
        , _directory(directory)
        , _context(context)
        , _tracked(tracked)
        , _directoryNode(std::make_shared<DirectoryNode>(context, symbolicDirectory(), nullptr))
        , _fileExecSpecsNode(std::make_shared<FileExecSpecsNode>(context, symbolicDirectory()))
        , _modified(true)
    {
        _context->nodes().add(_directoryNode);
        _context->nodes().add(_fileExecSpecsNode);
        _directoryNode->addPrerequisitesToContext();
    }

    FileRepository::~FileRepository() {
        stopWatching();
    }

    void FileRepository::startWatching() {
        if (_tracked) {
            if (_watcher == nullptr) {
                _watcher = std::make_shared<FileRepositoryWatcher>(this, _context);
            }
        }
    }

    void FileRepository::stopWatching() {
        if (_watcher != nullptr) {
            _watcher->stop();
            _watcher = nullptr;
        }
    }

    bool FileRepository::watching() const {
        return _watcher != nullptr;
    }

    void FileRepository::consumeChanges() {
        if (_watcher != nullptr) _watcher->consumeChanges();
    }

    bool FileRepository::hasChanged(std::filesystem::path const& path) {
        return (_watcher == nullptr) ? true : _watcher->hasChanged(path);
    }

    std::string const& FileRepository::name() const {
        return _name;
    }

    std::filesystem::path const& FileRepository::directory() const {
        return _directory;
    }

    bool FileRepository::isSymbolicPath(std::filesystem::path const& path) {
        return !repoNameFromPath(path).empty();
    }

    std::string FileRepository::repoNameFromPath(std::filesystem::path const& path) {
        static std::string empty;
        auto it = path.begin();
        if (it == path.end()) return empty;
        std::string repoComponent = (*it).string();
        std::size_t n = repoComponent.length();
        if (n < 3) return empty;
        if (repoComponent[0] != '@') return empty;
        if (repoComponent[1] != '@') return empty;
        return repoComponent.substr(2);
    }

    std::filesystem::path FileRepository::repoNameToSymbolicPath(std::string const& repoName) {
        std::filesystem::path p("@@" + repoName);
        return p;
    }

    bool FileRepository::lexicallyContains(std::filesystem::path const& path) const {
        bool contains;
        if (path.is_absolute()) {
            auto pit = path.begin();
            auto rit = _directory.begin();
            for (; rit != _directory.end(); rit++) {
                if ((pit != path.end()) && (*pit == *rit)) pit++;
                else break;
            }
            contains = rit == _directory.end();
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
        if (*symbolicPath.begin() != _directoryNode->name()) return absPath;
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

    std::shared_ptr<FileExecSpecsNode> FileRepository::fileExecSpecsNode() const {
        return _fileExecSpecsNode;
    }

    void FileRepository::clear() {
        _context->nodes().removeIfPresent(_fileExecSpecsNode);
        _context->nodes().removeIfPresent(_fileExecSpecsNode->configFileNode());
        _context->nodes().removeIfPresent(_directoryNode);
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
        streamer->stream(_tracked);
        streamer->stream(_directoryNode);
    }

    void FileRepository::prepareDeserialize() {
    }

    bool FileRepository::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!restored.insert(this).second) return false;
        _context = reinterpret_cast<ExecutionContext*>(context);
        if (_tracked) {
            if (_watcher != nullptr && _watcher->directory() != _directory) {
                stopWatching();
                startWatching();
            }
        } else {
            stopWatching();
        }
        return true;
    }
}