#include "pch.h"
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
		std::size_t oldSize = size();
		if (oldSize != newSize) {
			if (oldSize < newSize) { // add threads
				std::size_t nToAdd = newSize - oldSize;
				for (std::size_t i = 0; i < nToAdd; ++i) {
					std::stringstream tname;
					tname << _name << "_" << oldSize + i;
					auto t = std::make_shared<Thread>(_dispatcher, tname.str());
					_threads.push_back(t);
				}
			} else { // remove threads
				// request stop of threads to be removed
				for (std::size_t i = newSize; i < oldSize; ++i) _threads[i]->stop();
				// stop dispatcher to unblock dispatcher.pop(), allowing threads
				// that were stop-requested to finish.
				// Not nice: not-stopped threads will busy loop until dispatcher is 
				// restarted, i.e. until stopped threads have finished. This may
				// take a lot of time when stopped threads were executing long-lasting
				// delegates.
				_dispatcher->stop(); 
				// join with the threads that will finish.
				for (std::size_t i = newSize; i < oldSize; ++i) _threads[i]->join();
				// restart the dispatcher to allow not-stopped threads to resume 
				// processing of dispatched delegates.
				_dispatcher->start();
				// remove the finished threads from vector
				_threads.erase(_threads.begin() + newSize, _threads.end());
			}
		}
	}

	void ThreadPool::join() {
		if (0 < _threads.size()) {
			std::thread t(&ThreadPool::finishJoin, this);
			// Add a terminator delegate to dispatcher queue.
			// This terminator will be executed when all delegates in front of it
			// have executed or are still being executed.
			_dispatcher->push(Delegate<void>::CreateLambda([this]()
				{
					// size(0) must now be called to stop, join and remove all threads.
					// size(0) however cannot be called by the thread executing this
					// terminator delegate because size(0) will destruct all threads.
					// Therefore request another thread (t) to execute size(0).
					_joiner.push(Delegate<void>::CreateLambda([this]() { size(0); }));
				}));
			t.join(); // wait for thread t to complete execution of size(0)
		}
	}

	void ThreadPool::finishJoin() {
		Delegate<void> d = _joiner.pop();
		d.Execute();
	}
}