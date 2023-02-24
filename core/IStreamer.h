#pragma once

#include "IStreamable.h"
#include <memory>
#include <string>

namespace YAM
{
    // IStreamer is an interface that allows applications to write and read
    // (serialize and deserialize) objects to/from a stream of bytes.
    // The bi-directional interface allows one to use the same code for writing
    // and reading, thus ensuring that data is read in same order as it was 
    // written.
    // 
    // Note: sizeof(std::size_t) depends on whether application is a 32 or 64 
    // bit application. Streaming length words of type std::size_t makes it 
    // impossible for a 64 bit app to read streams produced by 32 bit app. 
    // Implementations must therefore document on how they stream std::size_t.
    //
    // Note: byte order of streamed data is chosen by implementations.
    //
    class __declspec(dllexport) IStreamer
    {
    public:
        IStreamer() {}

        virtual ~IStreamer() {}

        // Return whether the stream is in write or read mode.
        virtual bool writing() const = 0;
        bool reading() const { return !writing(); }

        // Stream 'nBytes' to/from 'bytes'.
        // Caller is responsible for allocating 'bytes'.
        virtual void stream(void* bytes, unsigned int nBytes) = 0;
        virtual void stream(bool& value) = 0;
        virtual void stream(float& value) = 0;
        virtual void stream(double& value) = 0;
        virtual void stream(int8_t& value) = 0;
        virtual void stream(uint8_t& value) = 0;
        virtual void stream(int16_t& value) = 0;
        virtual void stream(uint16_t& value) = 0;
        virtual void stream(int32_t& value) = 0;
        virtual void stream(uint32_t& value) = 0;
        virtual void stream(int64_t& value) = 0;
        virtual void stream(uint64_t& value) = 0;

        virtual void stream(std::string& value) = 0;
        virtual void stream(std::wstring& value) = 0;

        virtual void stream(IStreamable** streamable) = 0;
        virtual void stream(std::shared_ptr<IStreamable>& streamable) = 0;

        // Pre: deserializing
        // Return whether no more data can be read.
        virtual bool eos() = 0;

        // Pre: serializing
        // Flush buffer data, if any, to the bytes stream.
        virtual void flush() = 0;
    };
}