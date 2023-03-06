#pragma once

#include <memory>

namespace YAM
{
    class IStreamer;
    class IStreamable;

    // Interface for streaming dynamically allocated objects.
    // Implemenations shall stream copies of multiply referenced 
    // objects. Example:
    //     IStreamable* w1 = new SomeClass();
    //     ObjectWriter writer; // implements IObjectStreamer
    //     writer.stream(&w1);
    //     writer.stream(&w1);
    //     ObjectReader reader; // implements IObjectStreamer
    //     IStreamable* r1;
    //     IStreamable* r2;
    //     writer.stream(&r1);
    //     writer.stream(&r2);
    //     assert(w1->equals(r1));
    //     assert(r1 != r2); // r2 is a copy of w1
    //     assert(r1->equals(r2);
    //
    class __declspec(dllexport) IObjectStreamer
    {
    public:
        virtual ~IObjectStreamer() {}

        virtual void stream(IStreamer* streamer, IStreamable** object) = 0;
    };
}