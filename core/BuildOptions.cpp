#include "BuildOptions.h"
#include "IStreamer.h"
#include "FileSystem.h"

namespace YAM
{
    BuildOptions::BuildOptions() 
        : _clean(false)
        , _workingDir(FileSystem::canonicalPath(std::filesystem::current_path()))
        , _logAspects({ LogRecord::Aspect::Error, LogRecord::Aspect::Warning, LogRecord::Aspect::Progress})
        , _threads(0)
    { 

    }

    void BuildOptions::stream(IStreamer* streamer) {
        streamer->stream(_clean);
        streamer->stream(_workingDir);
        streamer->streamVector(_scope);
        streamer->stream(_threads);
        LogRecord::streamAspects(streamer, _logAspects);
    }
}
