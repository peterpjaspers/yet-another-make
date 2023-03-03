#pragma once

#include <cstdint>

namespace YAM
{
    // Interface that allows applications to write and read (serialize and 
    // deserialize) basic types to/from a stream of bytes.
    // The bi-directional interface allows one to use the same code for writing
    // and reading, thus ensuring that data is read in same order as it was 
    // written.
    class __declspec(dllexport) IValueStreamer
    {
    public:
        virtual ~IValueStreamer() {}

        // Return whether the stream is in write or read mode.
        virtual bool writing() const = 0;
        bool reading() const { return !writing(); }

        // Stream 'nBytes' to/from 'bytes'.
        // Caller is responsible for allocating 'bytes'.
        virtual void stream(void* bytes, unsigned int nBytes) = 0;

        // Stream value types
        virtual void stream(bool&) = 0;
        virtual void stream(float&) = 0;
        virtual void stream(double&) = 0;
        virtual void stream(int8_t&) = 0;
        virtual void stream(uint8_t&) = 0;
        virtual void stream(int16_t&) = 0;
        virtual void stream(uint16_t&) = 0;
        virtual void stream(int32_t&) = 0;
        virtual void stream(uint32_t&) = 0;
        virtual void stream(int64_t&) = 0;
        virtual void stream(uint64_t&) = 0;
    };
}