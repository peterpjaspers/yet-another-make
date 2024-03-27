#include "PriorityDispatcher.h"
#include "DispatcherFrame.h"

#include <limits>

namespace
{
    using namespace YAM;

    const uint32_t undefined = std::numeric_limits<uint32_t>::max();
}

namespace YAM
{
    PriorityDispatcher::PriorityDispatcher(uint32_t nPriorities)
        : _suspended(false)
        , _stopped(false)
        , _highest(undefined)
    {
        if (nPriorities > 1024) throw std::exception("too many priorities");
        for (uint32_t i = 0; i < nPriorities; ++i) {
            _queues.insert({ i, std::queue<Delegate<void>>()});
        }
    }

    uint32_t PriorityDispatcher::priorityOf(PriorityClass prio) const {
        uint32_t nPrios = nPriorities();
        switch (prio)
        {
            case PriorityClass::VeryHigh: return nPrios;
            case PriorityClass::High: return (nPrios * 3) / 4;
            case PriorityClass::Medium:return nPrios / 2;
            case PriorityClass::Low:return nPrios / 4;
            case PriorityClass::VeryLow: return 0;
            default: throw std::exception("invalid prio enum value");
        }
    }

    void PriorityDispatcher::push(Delegate<void>& action, uint32_t prio) {
        {
            std::lock_guard lk(_mtx);
            if (prio > maxPriority()) prio = maxPriority();
            _queues[prio].push(action);
            if (_highest == undefined || prio > _highest) _highest = prio;
        }
        _cv.notify_one();
    }

    void PriorityDispatcher::push(Delegate<void>&& action, uint32_t prio) {
        {
            std::lock_guard lk(_mtx);
            if (prio > maxPriority()) prio = maxPriority();
            _queues[prio].push(action);
            if (_highest == undefined || prio > _highest) _highest = prio;
        }
        _cv.notify_one();
    }

    void PriorityDispatcher::push(Delegate<void>& action, PriorityClass prio) {
        push(action, priorityOf(prio));
    }

    void PriorityDispatcher::push(Delegate<void>&& action, PriorityClass prio) {
        push(action, priorityOf(prio));
    }

    Delegate<void> PriorityDispatcher::pop() {
        std::unique_lock lk(_mtx);
        _cv.wait(lk, [this] { return (_highest != undefined && !_suspended) || _stopped; });
        Delegate<void> d;
        if (!_stopped) {
            auto& queue = _queues[_highest];
            d = queue.front();
            queue.pop();
            if (queue.empty()) {
                _highest = undefined;
                for (int32_t i = static_cast<int32_t>(nPriorities() - 1); i >= 0; --i) {
                    if (!_queues[i].empty()) {
                        _highest = i;
                        break;
                    }
                }
            }
        }
        return d;
    }

    std::size_t PriorityDispatcher::size() {
        std::lock_guard lk(_mtx);
        std::size_t size = 0;
        for (auto const& pair : _queues) size += pair.second.size();
        return size;
    }

    bool PriorityDispatcher::empty() {
        return size() == 0;
    }

    void PriorityDispatcher::suspend() {
        {
            std::lock_guard lk(_mtx);
            _suspended = true;
        }
        _cv.notify_all();
    }

    void PriorityDispatcher::resume() {
        {
            std::lock_guard lk(_mtx);
            _suspended = false;
        }
        _cv.notify_all();
    }

    bool PriorityDispatcher::suspended() {
        std::lock_guard lk(_mtx);
        return _suspended;
    }

    void PriorityDispatcher::start() {
        {
            std::lock_guard lk(_mtx);
            _stopped = false;
        }
        _cv.notify_all();
    }

    void PriorityDispatcher::stop() {
        {
            std::lock_guard lk(_mtx);
            _stopped = true;
        }
        _cv.notify_all();
    }

    bool PriorityDispatcher::stopped() {
        std::lock_guard lk(_mtx);
        return _stopped;
    }

    void PriorityDispatcher::popAndExecute() {
        Delegate<void> d = pop();
        if (d.IsBound()) d.Execute();
    }

    void PriorityDispatcher::run() {
        while (!stopped()) popAndExecute();
    }

    void PriorityDispatcher::run(IDispatcherFrame* frame) {
        while (!frame->stopped() && !stopped()) popAndExecute();
    }
}