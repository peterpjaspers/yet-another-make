#include "pch.h"
#include "Thread.h"

#include <exception>
#include <chrono>

namespace YAM
{
	Thread::Thread(Dispatcher* dispatcher, std::string const& name)
		: _dispatcher(dispatcher)
		, _name(name)
		, _thread(&Thread::main, this)
		, _stopRequested(false)
	{}

	Thread::~Thread() {
		if (joinable()) join();
	}

	std::string const& Thread::name() const {
		return _name;
	}

	void Thread::stop() {
		_stopRequested = true;
	}

	bool Thread::joinable() {
		return _thread.joinable();
	}

	void Thread::join() {
		_thread.join();
	}

	void Thread::main() {
		while (!_stopRequested) {
			Delegate<void> d = _dispatcher->pop();
			if (d.IsBound()) d.Execute();
		}
	}
}

