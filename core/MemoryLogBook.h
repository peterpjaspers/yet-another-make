#pragma once

#include "ILogBook.h"
#include <mutex>

namespace YAM
{
    class __declspec(dllexport) MemoryLogBook : public ILogBook
    {
    public:
        MemoryLogBook();

        // MT-safe
        void add(LogRecord const& record) override;

        // Not MT-safe, only to be used when no logging in progress
        std::vector<LogRecord> const& records();

    private:
        std::mutex _mutex;
        std::vector<LogRecord> _records;
    };
}

