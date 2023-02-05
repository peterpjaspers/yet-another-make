#pragma once

#include <atomic>

namespace YAM
{
    // A DispatcherFrame represents a loop that processes pending work items
    // (delegates) in a Dispatcher queue.
    // A loop is entered by pushing a DispatcherFrame onto a Dispatcher.
    // See Dispatcher::PushFrame.
    // At each iteration of the loop, the Dispatcher will check the Continue 
    // property on the DispatcherFrame to determine whether the loop should
    // continue or should stop.
    //
    class __declspec(dllexport) DispatcherFrame
    {
    public:
        DispatcherFrame() : _stop(false) {}
        void stop() { _stop = true; }
        bool stopped() const { return _stop; }

    private:
        std::atomic<bool> _stop;
    };
}