#pragma once

#include "Delegates.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace YAM
{
    // Thread-safe FIFO queue.
    class __declspec(dllexport) Dispatcher
    {
    public:
        // Construct dispatcher in !suspended() && started() state.
        Dispatcher();

        // Append element to end of queue
        void push(Delegate<void>&& newAction);

        // Block calling thread until (!empty() && !suspended()) || stopped(). 
        // When !stopped(): remove first element from queue and return it.
        // When stopped(): return delegate that is not bound.
        Delegate<void> pop();

        // Return number of elements in queue.
        std::size_t size();

        // Return whether queue is empty.
        bool empty();

        // Suspend dispatching until resumed. Also see pop()
        void suspend();
        void resume();
        bool suspended();

        // Start dispatching, see pop()
        void start();

        // Stop dispatching, see pop()
        void stop();

        // Return wether dispatcher is started/stopped.
        bool started() { return !stopped(); }
        bool stopped();

    private:
        bool _suspended;
        bool _stopped;
        std::queue<Delegate<void>> _queue;
        std::mutex _mtx;
        std::condition_variable _cv;
    };
}