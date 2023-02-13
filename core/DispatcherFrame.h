#pragma once

#include "Delegates.h"
#include <atomic>

namespace YAM
{
    // A dispatcher frame represents a loop that processes pending work items
    // (delegates) in a Dispatcher queue. See Dispatcher::run(DispatcherFrame).
    class __declspec(dllexport) IDispatcherFrame
    {
    public:
        // Return whether the loop must be ended.
        virtual bool stopped() const = 0;
    };

    //
    class __declspec(dllexport) DispatcherFrame : public IDispatcherFrame
    {
    public:

        DispatcherFrame() : _stop(false) {}

        // Set stopped() to return true at its next call.
        void stop() { _stop = true; }
        bool stopped() const override { return _stop; }

    private:
        std::atomic<bool> _stop;
    };

    class __declspec(dllexport) DispatcherFrameDelegate : public IDispatcherFrame
    {
    public:
        // Construct a dispather frame where stopped() returns
        // stopped.Execute().
        // Such a frame can be used to verify, after each event processed
        // by the loop, whether a certain condition became true.
        DispatcherFrameDelegate(Delegate<bool> stopped)
            : _stopped(stopped)
        {}

        bool stopped() const override { return _stopped.Execute(); }

    private:
        Delegate<bool> _stopped;
    };
}