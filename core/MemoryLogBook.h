#pragma once

#include "ILogBook.h"
#include <mutex>

namespace YAM
{
    class __declspec(dllexport) MemoryLogBook : public ILogBook
    {
    public:
        MemoryLogBook();

        void add(LogRecord const& record) override;

        std::vector<LogRecord> const& records() const;
        void clear();

        // Log all previously added records to given 'log'.
        void forwardTo(ILogBook& log) const;

    private:
        std::vector<LogRecord> _records;
    };
}

