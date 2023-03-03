#pragma once

#include "ObjectStreamer.h"

namespace YAM
{
    class __declspec(dllexport) BuildServiceMessageWriter : public ObjectWriter
    {
    protected:
        uint32_t getTypeId(IStreamable* object) const override;
    };

    class __declspec(dllexport) BuildServiceMessageReader : public ObjectReader
    {
    protected:
        IStreamable* readObject(IStreamer* streamer, uint32_t typeId) const override;
    };
}

