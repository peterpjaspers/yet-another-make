#include "Dispatcher.h"
#include "DispatcherFrame.h"


namespace YAM
{
    Dispatcher::Dispatcher()
        : _suspended(false)
        , _stopped(false)
    {}

    void Dispatcher::push(Delegate<void> & action) {
        {
            std::lock_guard lk(_mtx);
            _queue.push(action);
        }
        _cv.notify_one();
    }

    void Dispatcher::push(Delegate<void>&& action) {
        {
            std::lock_guard lk(_mtx);
            _queue.push(action);
        }
        _cv.notify_one();
    }

    Delegate<void> Dispatcher::pop() {
        std::unique_lock lk(_mtx);
        _cv.wait(lk, [this] { return (!_queue.empty() && !_suspended) || _stopped; });
        Delegate<void> d;
        if (!_stopped) {
             d = _queue.front();
            _queue.pop();
        }
        return d;
    }

    std::size_t Dispatcher::size() {
        std::lock_guard lk(_mtx);
        return _queue.size();
    }

    bool Dispatcher::empty() {
        std::lock_guard lk(_mtx);
        return _queue.empty();
    }

    void Dispatcher::suspend() {
        {
            std::lock_guard lk(_mtx);
            _suspended = true;
        }
        _cv.notify_all();
    }

    void Dispatcher::resume() {
        {
            std::lock_guard lk(_mtx);
            _suspended = false;
        }
        _cv.notify_all();
    }

    bool Dispatcher::suspended() {
        std::lock_guard lk(_mtx);
        return _suspended;
    }

    void Dispatcher::start() {
        {
            std::lock_guard lk(_mtx);
            _stopped = false;
        }
        _cv.notify_all();
    }

    void Dispatcher::stop() {
        {
            std::lock_guard lk(_mtx);
            _stopped = true;
        }
        _cv.notify_all();
    }

    bool Dispatcher::stopped() {
        std::lock_guard lk(_mtx);
        return _stopped;
    }

    void Dispatcher::popAndExecute() {
        Delegate<void> d = pop();
        if (d.IsBound()) d.Execute();
    }

    void Dispatcher::run() {
        while (!stopped()) popAndExecute();
    }

    void Dispatcher::run(IDispatcherFrame* frame) {
        while (!frame->stopped() && !stopped()) popAndExecute();
    }
}