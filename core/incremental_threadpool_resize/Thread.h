#pragma once

#include "Dispatcher.h"

#include <thread>
#include <string>
#include <atomic>

namespace YAM
{
	class __declspec(dllexport) Thread
	{
	public:
		Thread(Dispatcher* dispatcher, std::string const & name);

		// Destructs and joins with _thread if joinable.
		~Thread();

		std::string const& name() const;

		// Request the thread to stop.
		// Note that thread can only stop when unblocked on dispatcher. 
		void stop();

		bool joinable();
		void join();

	private:
		void main();

		Dispatcher* _dispatcher;
		std::string _name;
		std::thread _thread;
		std::atomic_bool _stopRequested;
	};
}

