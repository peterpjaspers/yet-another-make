#pragma once

#include "Dispatcher.h"

#include <thread>
#include <string>

namespace YAM
{
	class __declspec(dllexport) Thread
	{
	public:
		Thread(Dispatcher* dispatcher, std::string const & name);

		// Destructs and joins with _thread if joinable.
		~Thread();

		std::string const& name() const;
		Dispatcher* dispatcher() const;

		bool joinable();
		void join();

	private:
		void main();

		Dispatcher* _dispatcher;
		std::string _name;
		std::thread _thread;
	};
}

