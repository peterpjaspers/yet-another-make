#pragma once

#include <thread>
#include <string>

namespace YAM
{
    class PriorityDispatcher;
    class __declspec(dllexport) Thread
    {
    public:
        // Construct (and start) a thread that executes dispatcher->run().
        Thread(PriorityDispatcher* dispatcher, std::string const & name);
        ~Thread();

        std::string const& name() const;
        PriorityDispatcher* dispatcher() const;

        bool joinable();
        void join();

        // Return whether call is made in this thread.
        bool isThisThread() const;

    private:
        PriorityDispatcher* _dispatcher;
        std::string _name;
        std::thread _thread;
    };
}

