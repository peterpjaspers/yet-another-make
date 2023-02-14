#include "LogRecord.h"

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
            case SuspectBuildOrdering: return "SuspectBuildOrdering";
            case FileChanges: return "FileChanges";
            case BuildTimePrediction: return "BuildTimePrediction";
            default: throw std::exception("unknown aspect");
        }
    }
}