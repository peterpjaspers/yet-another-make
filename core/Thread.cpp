#include "Thread.h"

#include <exception>
#include <chrono>

namespace YAM
{
	Thread::Thread(Dispatcher* dispatcher, std::string const& name)
		: _dispatcher(dispatcher)
		, _name(name)
		, _thread(&Thread::main, this)
	{}

	Thread::~Thread() {
		if (joinable()) {
			join();
		}
	}

	std::string const& Thread::name() const {
		return _name;
	}

	bool Thread::joinable() {
		return _thread.joinable();
	}

	void Thread::join() {
		_thread.join();
	}

	void Thread::main() {
		while (!_dispatcher->stopped()) {
			Delegate<void> d = _dispatcher->pop();
			if (d.IsBound()) d.Execute();
		}
	}
}

