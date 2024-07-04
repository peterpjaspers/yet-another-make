#pragma once

#include "ISharedObjectStreamer.h"
#include <map>
#include <vector>

namespace YAM
{
    class IObjectStreamer;

    class __declspec(dllexport) SharedObjectWriter : public ISharedObjectStreamer
    {
    public:
        SharedObjectWriter(IObjectStreamer* writer);
        virtual ~SharedObjectWriter() {}

        void stream(IStreamer* writer, std::shared_ptr<IStreamable>& object) override;

    private:
        IObjectStreamer* _owriter;
        std::map<IStreamable*, uint32_t> _objects;
    };

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