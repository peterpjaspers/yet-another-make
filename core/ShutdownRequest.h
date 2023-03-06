#pragma once
#include "IStreamable.h"

namespace YAM
{
    class __declspec(dllexport) ShutdownRequest : public IStreamable
    {
    public:
        ShutdownRequest() {}
        ShutdownRequest(IStreamer* reader);

        static void setStreamableType(uint32_t type);
        // Inherited via IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
    };
}

