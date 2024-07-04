#pragma once

#include <memory>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <cstdint>

namespace YAM
{
    class IStreamable;

    // IStreamer is an interface that allows applications to write and read
    // (serialize and deserialize) simple value types and/or STL types and/or
    // custom types to/from a stream of bytes.
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
        virtual ~IStreamer() {}

        // Return whether the stream is in write or read mode.
        virtual bool writing() const = 0;
        bool reading() const { return !writing(); }

        virtual void stream(void* bytes, unsigned int nBytes) = 0;
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

        // Stream custom type
        virtual void stream(std::shared_ptr<IStreamable>& streamable) = 0;
        template <typename T> void stream(std::shared_ptr<T>& streamable);

        // Stream standard library types
        void stream(std::string&);
        void stream(std::wstring&);
        void stream(std::chrono::system_clock::time_point&);
        void stream(std::chrono::utc_clock::time_point&);
        void stream(std::filesystem::path&);
        template <typename T> void streamVector(std::vector<T>&);
        template <typename K, typename V> void streamMap(std::map<K,V>&);
    };
}

#include "IStreamer.inl"