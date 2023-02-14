#pragma once

#include "ILogBook.h"

namespace YAM
{
	class __declspec(dllexport) MemoryLogBook : public ILogBook
	{
	public:
		MemoryLogBook();

		void add(LogRecord const& record) override;
		std::vector<LogRecord> const& records();

	private:
		std::vector<LogRecord> _records;
	};
}

