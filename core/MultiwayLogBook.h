#pragma once

#include "ILogBook.h"
#include <memory>
#include <vector>

namespace YAM
{
	class __declspec(dllexport) MultiwayLogBook : public ILogBook
	{
	public:
		MultiwayLogBook();
		MultiwayLogBook(std::initializer_list<std::shared_ptr<ILogBook>> books);

		void add(std::shared_ptr<ILogBook> const& book);
		void add(LogRecord const& record) override;

	private:
		std::vector<std::shared_ptr<ILogBook>> _books;
	};
}
