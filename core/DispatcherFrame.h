#pragma once

#include <atomic>

namespace YAM
{
    // A DispatcherFrame represents a loop that processes pending work items
    // (delegates) in a Dispatcher queue. See Dispatcher::run(DispatcherFrame).
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