#pragma once

#include "Delegates.h"
#include "PriorityDispatcher.h"

#include <chrono>
#include <thread>

namespace YAM
{
    class PeriodicTimer
    {
    public:
        PeriodicTimer(
            std::chrono::system_clock::duration period,
            PriorityDispatcher& dispatcher,
            Delegate<void> const &callback);

        ~PeriodicTimer();
        void suspend();
        void resume();
        void stop();

    private:
        void run();

        std::chrono::system_clock::duration _period;
        PriorityDispatcher& _dispatcher;
        Delegate<void> _callback;
        std::mutex _mutex;
        std::condition_variable _cond;
        bool _stop;
        bool _suspend;
        std::thread _thread;
    };
}

