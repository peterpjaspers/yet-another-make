#pragma once

#include "TimePoint.h"
#include <string>

namespace YAM
{
    class __declspec(dllexport) LogRecord
    {
    public:
        enum Aspect {
            Error, // error in build file, command execution, etc.
            Warning, // 
            Progress, // successfull completion of command script
            ScriptOutput, // output of command script
            Script, // command script text
            Scope, // build scope info
            Performance, // time and memory usage
            InputFiles, // input files detected by a command
            SuspectBuildOrdering, // ordering depends on indirect prerequisites
            FileChanges, // files changed since previous build
            BuildTimePrediction // estimated remaining build time
        };

        LogRecord(
            LogRecord::Aspect _aspect,
            std::string const& _message,
            TimePoint const& _time = TimePoint()); // default is current time

        static std::string aspect2str(LogRecord::Aspect aspect);

        LogRecord::Aspect aspect;
        std::string message;
        TimePoint time;
    };
}

