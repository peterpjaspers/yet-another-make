#pragma once

#include "BuildRequest.h"
#include "BuildResult.h"
#include "StopBuildRequest.h"
#include "ShutdownRequest.h"
#include "LogRecord.h"

namespace YAM
{
    // Class that allocates unique type ids to the messages classes
    // used in the build service communication protocol. 
    class __declspec(dllexport) BuildServiceMessageTypes
    {
    public:
        enum MessageType {
            BuildRequest = 1,
            BuildResult = 2,
            StopBuildRequest = 3,
            ShutdownRequest = 4,
            LogRecord = 5
        };

        BuildServiceMessageTypes() {
            BuildRequest::setStreamableType(static_cast<uint32_t>(BuildRequest));
            BuildResult::setStreamableType(static_cast<uint32_t>(BuildResult));
            StopBuildRequest::setStreamableType(static_cast<uint32_t>(StopBuildRequest));
            ShutdownRequest::setStreamableType(static_cast<uint32_t>(ShutdownRequest));
            LogRecord::setStreamableType(static_cast<uint32_t>(LogRecord));
        }
    };
}

