#pragma once

#include "BuildServiceMessageStreamer.h"
#include "BuildServiceMessageTypes.h"

namespace
{
    BuildServiceMessageTypes types;
}

namespace YAM
{
    uint32_t BuildServiceMessageWriter::getTypeId(IStreamable* object) const {
        uint32_t tid = object->typeId();
        auto mtid = static_cast<BuildServiceMessageTypes::MessageType>(tid);
        bool valid = true;
        switch (mtid) {
        case BuildServiceMessageTypes::BuildRequest: break;
        case BuildServiceMessageTypes::StopBuildRequest: break;
        case BuildServiceMessageTypes::ShutdownRequest: break;
        default:
            valid = false;
        }
        if (!valid) throw std::exception("Build service protocol error: illegal object received");
        return tid;
    }

    IStreamable* BuildServiceMessageReader::readObject(IStreamer* streamer, uint32_t typeId) const {
        auto mtid = static_cast<BuildServiceMessageTypes::MessageType>(typeId);
        bool valid = false;
        IStreamable* object = nullptr;
        switch (mtid) {
        case BuildServiceMessageTypes::BuildResult:
        {
            object = new BuildResult(streamer);
            break;
        }
        case BuildServiceMessageTypes::LogRecord:
        {
            object = new LogRecord(streamer);
            break;
        }
        default:
            valid = false;
        }
        if (!valid) throw std::exception("Build service protocol error: illegal object sent");
        return object;
    }
}

