#include "FileRepositoryNode.h"
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
    FileRepositoryNode::FileRepositoryNode() {}

    FileRepositoryNode::FileRepositoryNode(
        ExecutionContext* context,
        std::string const& repoName,
        std::filesystem::path const& directory)
        : Node(context, repoName)
        , _repoName(repoName)
        , _type(FileRepositoryNode::RepoType::Ignore)
        , _directory(directory)
    {
        repoType(RepoType::Build);
    }

    FileRepositoryNode::~FileRepositoryNode() {
        stopWatching();
    }

    void FileRepositoryNode::startWatching() {
        if (_type != FileRepositoryNode::RepoType::Ignore) {
            if (_watcher == nullptr) {
                _watcher = std::make_shared<FileRepositoryWatcher>(this, context());
            }
        }
    }

    void FileRepositoryNode::stopWatching() {
        if (_watcher != nullptr) {
            _watcher->stop();
            _watcher = nullptr;
        }
    }

    bool FileRepositoryNode::watching() const {
        return _watcher != nullptr;
    }

    void FileRepositoryNode::consumeChanges() {
        if (_watcher != nullptr) _watcher->consumeChanges();
    }

    bool FileRepositoryNode::hasChanged(std::filesystem::path const& path) {
        return (_watcher == nullptr) ? true : _watcher->hasChanged(path);
    }

    std::string const& FileRepositoryNode::repoName() const {
        return _repoName;
    }
    FileRepositoryNode::RepoType FileRepositoryNode::repoType() const {
        return _type;
    }

    void FileRepositoryNode::repoType(RepoType newType) {
        if (_type != newType) {
            _type = newType;
            if (_type == RepoType::Ignore) {
                stopWatching();
                removeYourself();
            } else if (_directoryNode == nullptr) {
                _directoryNode = std::make_shared<DirectoryNode>(context(), symbolicDirectory(), nullptr);
                _fileExecSpecsNode = std::make_shared<FileExecSpecsNode>(context(), symbolicDirectory());
                context()->nodes().add(_directoryNode);
                context()->nodes().add(_fileExecSpecsNode);
                _directoryNode->addPrerequisitesToContext();
            }
            _hash = computeHash();
            modified(true);
            setState(Node::State::Dirty);
        }
    }

    void FileRepositoryNode::directory(std::filesystem::path const& dir) {
        if (_directory != dir) {
            _directory = dir;
            if (watching()) {
                stopWatching();
                startWatching();
            }
            _hash = computeHash();
            setState(Node::State::Dirty);

            modified(true);
            std::stringstream ss;
            ss
                << "Repository " << repoName() << " moved from " << _directory
                << " to " << dir << std::endl;
            LogRecord progress(LogRecord::Aspect::Progress, ss.str());
            context()->addToLogBook(progress);
        }
    }

    std::filesystem::path const& FileRepositoryNode::directory() const {
        return _directory;
    }

    bool FileRepositoryNode::isSymbolicPath(std::filesystem::path const& path) {
        return !repoNameFromPath(path).empty();
    }

    std::string FileRepositoryNode::repoNameFromPath(std::filesystem::path const& path) {
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

    std::filesystem::path FileRepositoryNode::repoNameToSymbolicPath(std::string const& repoName) {
        std::filesystem::path p("@@" + repoName);
        return p;
    }

    bool FileRepositoryNode::lexicallyContains(std::filesystem::path const& path) const {
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

    std::filesystem::path FileRepositoryNode::relativePathOf(std::filesystem::path const& absPath) const {
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

    std::filesystem::path FileRepositoryNode::symbolicPathOf(std::filesystem::path const& absPath) const {
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
            symPath /= repoNameToSymbolicPath(_repoName);
            for (; pit != absPath.end(); pit++) {
                symPath /= *pit;
            }
        }
        return symPath;
    }

    std::filesystem::path FileRepositoryNode::absolutePathOf(std::filesystem::path const& symbolicPath) const {
        std::filesystem::path absPath;
        if (*symbolicPath.begin() != symbolicDirectory()) return absPath;
        absPath = _directory;
        auto it = symbolicPath.begin();
        it++; // skip repo name component
        for (; it != symbolicPath.end(); it++) {
            absPath /= *it;
        }
        return absPath;
    }

    std::shared_ptr<DirectoryNode> FileRepositoryNode::directoryNode() const {
        return _directoryNode; 
    }

    std::shared_ptr<FileExecSpecsNode> FileRepositoryNode::fileExecSpecsNode() const {
        return _fileExecSpecsNode;
    }

    void FileRepositoryNode::removeYourself() {
        _directoryNode->clear();
        context()->nodes().removeIfPresent(_fileExecSpecsNode);
        context()->nodes().removeIfPresent(_fileExecSpecsNode->configFileNode());
        context()->nodes().removeIfPresent(_directoryNode);
        _fileExecSpecsNode = nullptr;
        _directoryNode = nullptr;
        modified(true);
    }

    XXH64_hash_t FileRepositoryNode::hash() const {
        return _hash;
    }

    XXH64_hash_t FileRepositoryNode::computeHash() const {
        std::vector<XXH64_hash_t> hashes;
        hashes.push_back(XXH64_string(_directory.string()));
        hashes.push_back(_type);
        return XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
    }

    void FileRepositoryNode::start() {
        Node::start();
        postCompletion(Node::State::Ok);
    }

    void FileRepositoryNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t FileRepositoryNode::typeId() const {
        return streamableTypeId;
    }

    void FileRepositoryNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(_directory);
        uint32_t type = static_cast<uint32_t>(_type);
        streamer->stream(type);
        if (streamer->reading()) _type = static_cast<FileRepositoryNode::RepoType>(type);
        streamer->stream(_directoryNode);
        streamer->stream(_fileExecSpecsNode);
    }

    void FileRepositoryNode::prepareDeserialize() {
        Node::prepareDeserialize();
    }

    bool FileRepositoryNode::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!Node::restore(context, restored)) return false;
        _repoName = name().string();
        _hash = computeHash();
        if (_watcher != nullptr && _watcher->directory() != _directory) {
            stopWatching();
            startWatching();
        }
        return true;
    }
}