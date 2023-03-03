#pragma once

#include "IStreamer.h"

namespace YAM
{
    class IValueStreamer;
    class ISharedObjectStreamer;

    class __declspec(dllexport) Streamer : public IStreamer
    {
    public:
        Streamer(IValueStreamer* valueStreamer, ISharedObjectStreamer* objectStreamer);
        virtual ~Streamer() {}

        // Start inherit from IStreamer
        bool writing() const override;
        void stream(void* bytes, unsigned int nBytes) override;
        void stream(bool&) override;
        void stream(float&) override;
        void stream(double&) override;
        void stream(int8_t&) override;
        void stream(uint8_t&) override;
        void stream(int16_t&) override;
        void stream(uint16_t&) override;
        void stream(int32_t&) override;
        void stream(uint32_t&) override;
        void stream(int64_t&) override;
        void stream(uint64_t&) override;
        virtual void stream(std::shared_ptr<IStreamable>& streamable) override;
        // End inherit from IStreamer

    private:
        IValueStreamer* _valueStreamer;
        ISharedObjectStreamer* _objectStreamer;
    };
}