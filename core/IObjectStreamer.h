#pragma once

#include <memory>

namespace YAM
{
    class IStreamer;
    class IStreamable;

    // Interface for streaming dynamically allocated objects.
    class __declspec(dllexport) IObjectStreamer
    {
    public:
        virtual ~IObjectStreamer() {}

        virtual void stream(IStreamer* streamer, IStreamable** object) = 0;
    };
}