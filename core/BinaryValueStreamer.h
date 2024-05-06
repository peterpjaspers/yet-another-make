#pragma once

#include "IValueStreamer.h"

namespace YAM
{
    class IOutputStream;
    class __declspec(dllexport) BinaryValueWriter : public IValueStreamer
    {
    public:
        BinaryValueWriter(IOutputStream* oStream);

        bool writing() const override { return true; }
        void stream(void* bytes, unsigned int nBytes) override;
        void stream(bool& value) override;
        void stream(float& value) override;
        void stream(double& value) override;
        void stream(int8_t& value) override;
        void stream(uint8_t& value) override;
        void stream(int16_t& value) override;
        void stream(uint16_t& value) override;
        void stream(int32_t& value) override;
        void stream(uint32_t& value) override;
        void stream(int64_t& value) override;
        void stream(uint64_t& value) override;

        void close() override;

    private:
        IOutputStream* _stream;
    };
}

namespace YAM
{
    class IInputStream;
    class __declspec(dllexport) BinaryValueReader : public IValueStreamer
    {
    public:
        BinaryValueReader(IInputStream* ioStream);

        bool writing() const override { return false; }
        void stream(void* bytes, unsigned int nBytes) override;
        void stream(bool& value) override;
        void stream(float& value) override;
        void stream(double& value) override;
        void stream(int8_t& value) override;
        void stream(uint8_t& value) override;
        void stream(int16_t& value) override;
        void stream(uint16_t& value) override;
        void stream(int32_t& value) override;
        void stream(uint32_t& value) override;
        void stream(int64_t& value) override;
        void stream(uint64_t& value) override;

    private:
        IInputStream* _stream;
    };
}
