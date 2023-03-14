#pragma once

#include <cstdint>

namespace YAM
{ 
    class IStreamer;

    class __declspec(dllexport) IStreamable
    {
    public:
        IStreamable() = default;

        virtual ~IStreamable() {}

        // Return an id that identifies the type (class) of the streamable.
        // This id is intended to be used by IStreamer to encode type of
        // streamable in the stream.
        virtual uint32_t typeId() const = 0;

        // Stream member variables to given streamer.
        virtual void stream(IStreamer* streamer) = 0;
    };
}