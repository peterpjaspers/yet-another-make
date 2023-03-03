#pragma once

#include "IStreamable.h"
#include "TimePoint.h"
#include <string>

namespace YAM
{
    class __declspec(dllexport) LogRecord : public IStreamable
    {
    public:
        enum Aspect {
            Error = 0, // error in build file, command execution, etc.
            Warning = 1, // 
            Progress = 2, // successfull completion of command script
            ScriptOutput = 3, // output of command script
            Script = 4, // command script text
            Scope = 5, // build scope info
            Performance = 6, // time and memory usage
            InputFiles = 7, // input files detected by a command
            SuspectBuildOrdering = 8, // ordering depends on indirect prerequisites
            FileChanges = 9, // files changed since previous build
            BuildTimePrediction = 10 // estimated remaining build time
        };

        LogRecord(
            LogRecord::Aspect _aspect,
            std::string const& _message,
            TimePoint const& _time = TimePoint()); // default is current time

        LogRecord(IStreamer* reader) : IStreamable(reader) {}

        static std::string aspect2str(LogRecord::Aspect aspect);

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;

        LogRecord::Aspect aspect;
        std::string message;
        TimePoint time;
    };
}

