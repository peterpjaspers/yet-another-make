#include "BuildRequest.h"
#include "IStreamer.h"

namespace
{
    uint32_t _streamableType = 0;
}

namespace YAM
{
    BuildRequest::BuildRequest(IStreamer* reader) {
        stream(reader);
    }

    BuildRequest::BuildRequest()
        : _logAspects(LogRecord::allAspects())
    {}

    // Set/get the directory from which the build is started.
    void BuildRequest::repoDirectory(std::filesystem::path const& directory) {
        _repoDirectory = directory;
    }

    std::filesystem::path const& BuildRequest::repoDirectory() {
        return _repoDirectory;
    }

    void BuildRequest::repoName(std::string const& newName) {
        _repoName = newName;
    }

    std::string const& BuildRequest::repoName() const {
        return _repoName;
    }

    void BuildRequest::addToScope(std::filesystem::path const& path) {
        _pathsInScope.push_back(path);
    }

    std::vector<std::filesystem::path> const& BuildRequest::pathsInScope() {
        return _pathsInScope;
    }

    void BuildRequest::logAspects(std::vector<LogRecord::Aspect> const& aspects) {
        _logAspects = aspects;
    }

    std::vector<LogRecord::Aspect>const& BuildRequest::logAspects() const {
        return _logAspects;
    }

    void BuildRequest::setStreamableType(uint32_t type) {
        _streamableType = type;
    }

    uint32_t BuildRequest::typeId() const {
        return _streamableType;
    }

    void BuildRequest::stream(IStreamer* streamer) {
        streamer->stream(_repoDirectory);
        streamer->stream(_repoName);
        streamer->streamVector(_pathsInScope);
        LogRecord::streamAspects(streamer, _logAspects);
    }
}