#pragma once

#include "IStreamable.h"
#include "LogRecord.h"

#include <filesystem>
#include <vector>

namespace YAM
{
    struct __declspec(dllexport) BuildOptions : public IStreamable
    {
        BuildOptions();

        // Whether to delete all previously generated output files
        bool _clean;

        std::filesystem::path _workingDir;

        // Only build the files specified in _scope.
        // Path may be absolute, symbolic or relative to _workingDir.
        // Paths may contain glob characters.
        std::vector<std::filesystem::path> _scope;

        // Only log records whose aspect is in _logAspects.
        std::vector<LogRecord::Aspect> _logAspects;

        // Inherited via IStreamable
        uint32_t typeId() const override { throw std::runtime_error("not supported"); }
        void stream(IStreamer* streamer) override;
    };
}

