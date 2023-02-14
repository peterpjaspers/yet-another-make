#pragma once

#include "LogRecord.h"

namespace YAM
{
    class __declspec(dllexport) ILogBook
    {
    public:
        virtual void add(LogRecord const& record) = 0;
    };
}
