#pragma once

#include <thread>
#include <string>
#include <chrono>

namespace YAM
{
    class PriorityDispatcher;
    class __declspec(dllexport) Thread
    {
    public:
        // Construct (and start) a thread that executes dispatcher->run().
        Thread(PriorityDispatcher* dispatcher, std::string const & name);
        ~Thread();

        void run();
        std::string const& name() const;
        PriorityDispatcher* dispatcher() const;

        bool joinable();
        void join();

        // Return whether call is made in this thread.
        bool isThisThread() const;

        // Return the time spent in this thread since the last call
        // to this function.
        std::chrono::nanoseconds timeUsage() {
            auto tmp = _executeDuration;
            _executeDuration = std::chrono::nanoseconds::zero();
            return tmp;
        }

    private:
        PriorityDispatcher* _dispatcher;
        std::string _name;
        std::thread _thread;
        std::chrono::nanoseconds _executeDuration;
    };
}

