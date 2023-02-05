#pragma once

#include "Delegates.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace YAM
{
    class DispatcherFrame;

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

        // Pop a delegate from queue and execute it.
        void popAndExecute();

        // Execute the following loop: 
        //     while (!stopped()) popAndExecute();
        void run();

        // Execute the following loop: 
        //     while (!frame.stopped() && !stopped()) popAndExecute();
        //
        // This function allows reentrant calls to be finished without having
        // to stop the entire dispatcher. It can be used to run the event loop
        // until a specific event occurred.
        // Example:
        //     public void doEvents(Dispatcher* dispatcher) {
        //         DispatcherFrame frame;
        //         // queue a delegate that stops the frame.
        //         dispatcher->push([frame]() { frame->stop(); });
        //         // block until all events (delegates) until and including
        //         // the stop event have been processed.
        //         dispatcher->run(frame);
        //     }
        void run(DispatcherFrame* frame);

    private:
        bool _suspended;
        bool _stopped;
        std::queue<Delegate<void>> _queue;
        std::mutex _mtx;
        std::condition_variable _cv;
    };
}