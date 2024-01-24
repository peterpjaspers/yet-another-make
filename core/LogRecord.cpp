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

    std::vector<LogRecord::Aspect>const& LogRecord::allAspects() {
        static std::vector<LogRecord::Aspect> all;
        if (all.empty()) {
            all.push_back(Aspect::Error);
            all.push_back(Aspect::Warning);
            all.push_back(Aspect::Progress);
            all.push_back(Aspect::ScriptOutput);
            all.push_back(Aspect::Script);
            all.push_back(Aspect::Scope);
            all.push_back(Aspect::Performance);
            all.push_back(Aspect::InputFiles);
            all.push_back(Aspect::IgnoredInputFiles);
            all.push_back(Aspect::FileChanges);
            all.push_back(Aspect::DirectoryChanges);
            all.push_back(Aspect::BuildTimePrediction);
            //all.push_back(Aspect::BuildState);
        }
        return all;
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
            case FileChanges: return "FileChanges";
            case DirectoryChanges: return "DirectoryChanges";
            case BuildTimePrediction: return "BuildTimePrediction";
            case BuildState: return "BuildState";
            default: return "unknown aspect";
        }
    }

    void LogRecord::setStreamableType(uint32_t type) {
        _streamableType = type;
    }

    uint32_t LogRecord::typeId() const {
        return _streamableType;
    }

    void LogRecord::streamAspect(IStreamer* streamer, LogRecord::Aspect& aspect) {
        uint32_t a;
        if (streamer->writing()) a = static_cast<uint32_t>(aspect);
        streamer->stream(a);
        if (streamer->reading()) aspect = static_cast<Aspect>(a);
    }

    void LogRecord::streamAspects(IStreamer* streamer, std::vector<LogRecord::Aspect>& aspects) {
        std::vector<uint32_t> v;
        if (streamer->writing()) for (auto a : aspects) v.push_back(static_cast<uint32_t>(a));
        streamer->streamVector(v);
        if (streamer->reading()) for (auto a : v) aspects.push_back(static_cast<Aspect>(a));
    }

    void LogRecord::stream(IStreamer* streamer) {
        streamAspect(streamer, aspect);
        streamer->stream(message);
        time.stream(streamer);
    }
}