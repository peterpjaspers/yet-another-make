#include "MemoryLogBook.h"

namespace YAM
{
    MemoryLogBook::MemoryLogBook() {}

    void MemoryLogBook::add(LogRecord const& record) {
        ILogBook::add(record);
        _records.push_back(record); 
    }

    std::vector<LogRecord> const& MemoryLogBook::records() const {
        return _records;
    }

    void MemoryLogBook::forwardTo(ILogBook& log) const {
        for (auto const& r : _records) log.add(r);
    }
    void MemoryLogBook::clear() {
        _records.clear();
    }
}
