#include "BuildOptions.h"
#include "IStreamer.h"

namespace YAM
{
    BuildOptions::BuildOptions() 
        : _clean(false)
        , _workingDir(std::filesystem::current_path())
        , _logAspects({ LogRecord::Aspect::Error, LogRecord::Aspect::Warning})
    { }

    void BuildOptions::stream(IStreamer* streamer) {
        streamer->stream(_clean);
        streamer->stream(_workingDir);
        streamer->streamVector(_scope);
        LogRecord::streamAspects(streamer, _logAspects);
    }
}
