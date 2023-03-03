#pragma once

#include <memory>

namespace YAM
{
    class IStreamer;
    class IStreamable;

    // Interface for streaming dynamically allocated objects.
    class __declspec(dllexport) ISharedObjectStreamer
    {
    public:
        virtual ~ISharedObjectStreamer() {}

        virtual void stream(IStreamer* streamer, std::shared_ptr<IStreamable>& object) = 0;
    };
}