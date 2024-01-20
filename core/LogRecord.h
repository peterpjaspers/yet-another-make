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
            Error,
            Warning,
            Progress,
            ScriptOutput,         // output of command script
            Script,               // command script text
            Scope,                // build scope info
            Performance,          // time and memory usage
            InputFiles,           // input files of a command
            IgnoredInputFiles,    // ignored input files of a commansd
            FileChanges,          // files changed since previous build
            DirectoryChanges,     // directories changed since previous build
            BuildTimePrediction   // estimated remaining build time
        };

        LogRecord(
            LogRecord::Aspect _aspect,
            std::string const& _message,
            TimePoint const& _time = TimePoint()); // default is current time

        LogRecord(IStreamer* reader);

        static std::vector<LogRecord::Aspect>const& allAspects();
        static std::string aspect2str(LogRecord::Aspect aspect);
        static void setStreamableType(uint32_t type);
        static void streamAspect(IStreamer* streamer, LogRecord::Aspect& aspect);
        static void streamAspects(IStreamer* streamer, std::vector<LogRecord::Aspect>& aspects);

        uint32_t a;
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;

        LogRecord::Aspect aspect;
        std::string message;
        TimePoint time;
    };
}

