#include "MemoryLogBook.h"

namespace YAM
{
	MemoryLogBook::MemoryLogBook() {}

	void MemoryLogBook::add(LogRecord const& record) {
		_records.push_back(record); 
	}

	std::vector<LogRecord> const& MemoryLogBook::records() { 
		return _records; 
	}
}