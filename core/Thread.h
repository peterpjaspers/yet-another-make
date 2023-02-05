#pragma once

#include <thread>
#include <string>

namespace YAM
{
	class Dispatcher;
	class __declspec(dllexport) Thread
	{
	public:
		// Construct (and start) a thread that executes dispatcher->run().
		Thread(Dispatcher* dispatcher, std::string const & name);
		~Thread();

		std::string const& name() const;
		Dispatcher* dispatcher() const;

		bool joinable();
		void join();

	private:
		Dispatcher* _dispatcher;
		std::string _name;
		std::thread _thread;
	};
}

