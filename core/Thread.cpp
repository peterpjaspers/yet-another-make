#include "Thread.h"
#include "PriorityDispatcher.h"

namespace
{
    void run(YAM::PriorityDispatcher* dispatcher) {
        dispatcher->run();
    }
}

namespace YAM
{
    Thread::Thread(PriorityDispatcher* dispatcher, std::string const& name)
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

    PriorityDispatcher* Thread::dispatcher() const {
        return _dispatcher;
    }

    bool Thread::joinable() {
        return _thread.joinable();
    }

    void Thread::join() {
        _thread.join();
    }

    bool Thread::isThisThread() const {
        auto id = _thread.get_id();
        auto tid = std::this_thread::get_id();
        return id == tid;
    }
}

