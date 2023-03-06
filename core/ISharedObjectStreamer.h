#pragma once

#include <memory>

namespace YAM
{
    class IStreamer;
    class IStreamable;

    // Interface for streaming dynamically allocated objects.
    // Implemenations shall support streaming of multiply reference objects.
    // Example:
    //     std::shared_ptr<IStreamable> w1 = std::make_shared<SomeClass>();
    //     SharedObjectWriter writer; // implements ISharedObjectStreamer
    //     writer.stream(w1);
    //     writer.stream(w1);
    //     SharedObjectReader reader; // implements ISharedObjectStreamer
    //     std::shared_ptr<IStreamable> r1, r2;
    //     writer.stream(r1);
    //     writer.stream(r2);
    //     assert(w1.get()->equals(r1.get());
    //     assert(r1 == r2);
    //
    class __declspec(dllexport) ISharedObjectStreamer
    {
    public:
        virtual ~ISharedObjectStreamer() {}

        virtual void stream(IStreamer* streamer, std::shared_ptr<IStreamable>& object) = 0;
    };
}