#include "ThreadPool.h"

namespace YAM
{
	ThreadPool::ThreadPool(Dispatcher* dispatcher, std::string const& name, std::size_t nThreads)
		: _dispatcher(dispatcher)
		, _name(name)
	{
		size(nThreads);
	}

	ThreadPool::~ThreadPool() {
		size(0);
	}

	std::size_t ThreadPool::size() const {
		return  _threads.size();
	}

	void ThreadPool::size(std::size_t newSize) {
		if (size() != newSize) {
			// Stopping the dispatcher will stop all threads:
			//		- threads busy executing a delegate run the delegate to completion and
			//		  then stop.
			//		- threads blocked on pop-ing a delegate from the dispatcher will
			//		  unblock and stop. 
			_dispatcher->stop();

			// Clearing the threads vector will join with the threads (see Thread::~Thread)
			// and hence block the calling thread for the time needed to complete the 
			// delegates that were executing at time of size adjustment request. 
			_threads.clear(); 

			// Newly created threads will finish immediately when the dispatcher is in
			// stopped state. Hence restart dispatcher before creating new threads.
			_dispatcher->start();

			for (std::size_t i = 0; i < newSize; ++i) {
				std::stringstream tname;
				tname << _name << "_" << i;
				auto t = std::make_shared<Thread>(_dispatcher, tname.str());
				_threads.push_back(t);
			}
		}
	}

	void ThreadPool::join() {
		if (0 < _threads.size()) {
			// Finish all pending work before stopping dispatcher
			_dispatcher->push(Delegate<void>::CreateRaw(_dispatcher, &Dispatcher::stop));
			_threads.clear(); // join with stopped threads
		}
	}
}