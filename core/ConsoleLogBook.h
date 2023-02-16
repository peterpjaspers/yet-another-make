#pragma once

#include "ILogBook.h"
#include "BasicOStreamLogBook.h"
#include <memory>

namespace YAM
{
	class Console;

	class __declspec(dllexport) ConsoleLogBook : public BasicOStreamLogBook
	{
	public:
		ConsoleLogBook();

		void add(LogRecord const& record) override;

	private:
		void setColors(LogRecord::Aspect aspect);

		std::shared_ptr<Console> _console;
	};
}

