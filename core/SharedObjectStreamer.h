#pragma once

#include "ISharedObjectStreamer.h"
#include <map>
#include <vector>

namespace YAM
{
    class IObjectStreamer;

    // Base class capable of writing (serializing)  objects into a stream.
    class __declspec(dllexport) SharedObjectWriter : public ISharedObjectStreamer
    {
    public:
        SharedObjectWriter(IObjectStreamer* writer);
        virtual ~SharedObjectWriter() {}

        void stream(IStreamer* writer, std::shared_ptr<IStreamable>& object) override;

    private:
        IObjectStreamer* _owriter;
        std::map<IStreamable*, int> _objects;
    };

    // Base class capable of reading (deserializing)  objects into a stream.
    // Derived class must implement createInstance().
    class __declspec(dllexport) SharedObjectReader : public ISharedObjectStreamer
    {
    public:
        SharedObjectReader(IObjectStreamer* reader);
        virtual ~SharedObjectReader() {}

        void stream(IStreamer* reader, std::shared_ptr<IStreamable>& object) override;
    private:
        IObjectStreamer* _oreader;
        std::vector<std::shared_ptr<IStreamable>> _objects;
    };

}