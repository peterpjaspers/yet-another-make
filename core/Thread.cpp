#include "Thread.h"
#include "Dispatcher.h"

namespace
{
	void run(YAM::Dispatcher* dispatcher) {
		dispatcher->run();
	}
}

namespace YAM
{
	Thread::Thread(Dispatcher* dispatcher, std::string const& name)
		: _dispatcher(dispatcher)
		, _name(name)
		, _thread(&run, _dispatcher)
	{}

	Thread::~Thread() {
		if (joinable()) {
			join();
		}
	}

	std::string const& Thread::name() const {
		return _name;
	}

	Dispatcher* Thread::dispatcher() const {
		return _dispatcher;
	}

	bool Thread::joinable() {
		return _thread.joinable();
	}

	void Thread::join() {
		_thread.join();
	}
}

