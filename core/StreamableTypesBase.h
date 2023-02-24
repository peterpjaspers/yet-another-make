#pragma once

#include "IStreamableTypes.h"
#include "IStreamer.h"

namespace YAM
{
    template <typename TID>
    class __declspec(dllexport) StreamableTypesBase : public IStreamableTypes
    {
    public:
        virtual ~StreamableTypesBase() { }

        void streamType(IStreamer* streamer, IStreamable** streamable) const override
        {
            TID type_;
            if (streamer->writing()) type_ = getType(*streamable);
            streamer->stream(type_);
            if (streamer->reading()) *streamable = createInstance(type_);
        }

    protected:
        virtual TID getType(IStreamable* streamable) const = 0;
        virtual IStreamable* createInstance(TID const& typeId) const = 0;

    };

    class __declspec(dllexport) StreamableTypesByIdBase : public StreamableTypesBase<uint32_t>
    {
    protected:
        uint32_t getType(IStreamable* streamable) const override { return streamable->typeId(); }
    };

    class __declspec(dllexport) StreamableTypesByNameBase : public StreamableTypesBase<std::string>
    {
    protected:
        std::string getType(IStreamable* streamable) const override { return streamable->typeName(); }
    };
}