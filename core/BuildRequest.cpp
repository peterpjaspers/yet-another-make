#include "BuildRequest.h"
#include "IStreamer.h"

namespace
{
    uint32_t _streamableType = 0;
}

namespace YAM
{
    BuildRequest::BuildRequest(RequestType type)
        : _type(type)
    {}

    // Set/get the build command type.
    void BuildRequest::requestType(BuildRequest::RequestType type) {
        _type = type;
    }

    BuildRequest::RequestType BuildRequest::requestType() const {
        return _type;
    }

    // Set/get the directory from which the build is started.
    void BuildRequest::directory(std::filesystem::path const& directory) {
        _directory = directory;
    }

    std::filesystem::path const& BuildRequest::directory() {
        return _directory;
    }

    void BuildRequest::addToScope(std::filesystem::path const& path) {
        _pathsInScope.push_back(path);
    }

    std::vector<std::filesystem::path> const& BuildRequest::pathsInScope() {
        return _pathsInScope;
    }

    void BuildRequest::setStreamableType(uint32_t type) {
        _streamableType = type;
    }

    uint32_t BuildRequest::typeId() const {
        return _streamableType;
    }

    void BuildRequest::stream(IStreamer* streamer) {
        int32_t t = static_cast<int32_t>(_type);
        streamer->stream(t);
        if (streamer->reading()) _type = static_cast<RequestType>(t);
        streamer->stream(_directory);
        streamer->streamVector(_pathsInScope);
    }
}