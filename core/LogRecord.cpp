#include "LogRecord.h"
#include "IStreamer.h"

namespace
{
    uint32_t _streamableType = 0;
}

namespace YAM
{
    LogRecord::LogRecord(
        LogRecord::Aspect _aspect,
        std::string const& _message,
        TimePoint const& _time)
        : aspect(_aspect)
        , message(_message)
        , time(_time)
    {}

    LogRecord::LogRecord(IStreamer* reader) {
        stream(reader);
    }

    std::string LogRecord::aspect2str(LogRecord::Aspect aspect) {
        switch (aspect) {
            case Error: return "Error";
            case Warning: return "Warning";
            case Progress: return "Progress";
            case ScriptOutput: return "ScriptOutput";
            case Script: return "Script";
            case Scope: return "Scope";
            case Performance: return "Performance";
            case InputFiles: return "InputFiles";
            case IgnoredInputFiles: return "Ignored inputFiles";
            case SuspectBuildOrdering: return "SuspectBuildOrdering";
            case FileChanges: return "FileChanges";
            case BuildTimePrediction: return "BuildTimePrediction";
            default: throw std::exception("unknown aspect");
        }
    }

    void LogRecord::setStreamableType(uint32_t type) {
        _streamableType = type;
    }

    uint32_t LogRecord::typeId() const {
        return _streamableType;
    }

    void LogRecord::stream(IStreamer* streamer) {
        uint32_t a;
        if (streamer->writing()) a = static_cast<uint32_t>(aspect);
        streamer->stream(a);
        if (streamer->reading()) aspect = static_cast<Aspect>(a);
        streamer->stream(message);
        time.stream(streamer);
    }
}