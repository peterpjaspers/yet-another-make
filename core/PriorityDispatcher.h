#pragma once

#include "Delegates.h"
#include "PriorityClass.h"

#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>

namespace YAM
{
    class IDispatcherFrame;

    // Thread-safe FIFO priority queue.
    // Priorities range from 0 up to a given maximum.
    class __declspec(dllexport) PriorityDispatcher
    {
    public:
        // Construct dispatcher for priorities in range [0, nPriorities-1] 
        // !suspended() && started() state.
        // Memory complexity for empty queue is O(nPriorities).
        PriorityDispatcher(uint32_t nPriorities);

        uint32_t nPriorities() const { return static_cast<uint32_t>(_queues.size()); }
        uint32_t maxPriority() const { return nPriorities() - 1; }
        uint32_t priorityOf(PriorityClass prio) const;

        // Append element to end of queue for given priority.
        void push(Delegate<void>& newAction, uint32_t prio);
        void push(Delegate<void>&& newAction, uint32_t prio);
        void push(Delegate<void>& newAction, PriorityClass prio = PriorityClass::Medium);
        void push(Delegate<void>&& newAction, PriorityClass prio = PriorityClass::Medium);

        // Block calling thread until (!empty() && !suspended()) || stopped(). 
        // When !stopped(): remove+return highest priority element from queue.
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
        void run(IDispatcherFrame* frame);

    private:
        bool _suspended;
        bool _stopped;
        std::map<uint32_t, std::queue<Delegate<void>>> _queues;
        uint32_t _highest; // prio of queued delegate with highest priority
        std::mutex _mtx;
        std::condition_variable _cv;
    };
}