#pragma once

#include "Dispatcher.h"
#include "Thread.h"

#include <vector>
#include <string>
#include <memory>
#include <thread>

namespace YAM
{
	class __declspec(dllexport) ThreadPool
	{
	public:
		ThreadPool(Dispatcher* dispatcher, std::string const& name, std::size_t nThreads);
		~ThreadPool();

		// Return the number of threads in the pool.
		std::size_t size() const;

		// Adjust the number of threads in the pool.
		// Processing of delegates continues while size is adjusted.
		// Setting the size to 0 runs delegates-in-progress to completion
		// and then stops all processing. Not executed delegates will
		// remain in the dispatcher queue.
		void size(std::size_t newSize);

		// Join with all threads in pool.
		// Block caller until all dispatched delegates have been executed
		// by threads in thread pool, then stop dispatcher and join with threads
		// in pool.
		// Post: threadCount() == 0
		void join();

	private:
		void finishJoin();

		Dispatcher* _dispatcher;
		std::string _name;
		std::vector<std::shared_ptr<Thread>> _threads;
		Dispatcher _joiner;
	};
}

