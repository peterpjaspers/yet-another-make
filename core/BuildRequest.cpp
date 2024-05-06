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

    BuildRequest::BuildRequest() {}

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

    void BuildRequest::options(BuildOptions const& newOptions) {
        _options = newOptions;
    }

    BuildOptions const& BuildRequest::options() const {
        return _options;
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
        _options.stream(streamer);
    }
}