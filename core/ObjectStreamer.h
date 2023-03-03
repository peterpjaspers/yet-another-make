#pragma once

#include "IObjectStreamer.h"
#include "IStreamer.h"
#include "IStreamable.h"

namespace YAM
{
    class __declspec(dllexport) ObjectWriter : public IObjectStreamer
    {
    public:
        ObjectWriter() = default;
        virtual ~ObjectWriter() {}

        void stream(IStreamer* writer, IStreamable** object) override {
            uint32_t typeId = getTypeId(*object);
            writer->stream(typeId);
            (*object)->stream(writer);
        }

    protected:
        // Return the type of given object
        virtual uint32_t getTypeId(IStreamable* object) const = 0;
    };

    class __declspec(dllexport) ObjectReader : public IObjectStreamer
    {
    public:
        ObjectReader() = default;
        virtual ~ObjectReader() {}

        void stream(IStreamer* reader, IStreamable** object) override {
            uint32_t typeId;
            reader->stream(typeId);
            *object = readObject(reader, typeId);
        }

    protected:
        // Instantiate an object of given type and stream the object's member 
        // variables from given streamer.
        virtual IStreamable* readObject(IStreamer* streamer, uint32_t typeId) const = 0;
    };

}