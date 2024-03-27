#include "PeriodicTimer.h"
#include "PriorityDispatcher.h"

namespace YAM
{

    PeriodicTimer::PeriodicTimer(
        std::chrono::system_clock::duration period,
        PriorityDispatcher& dispatcher,
        Delegate<void> const &callback)
        : _period(period)
        , _dispatcher(dispatcher)
        , _callback(callback)
        , _stop(false)
        , _suspend(true)
        , _thread(&PeriodicTimer::run, this)
    {}

    PeriodicTimer::~PeriodicTimer() { 
        stop(); 
    }

    void PeriodicTimer::stop() {
        {
            std::lock_guard lk(_mutex);
            _stop = true;
        }
        _cond.notify_one();
        _thread.join();
    }

    void PeriodicTimer::suspend() {
        {
            std::lock_guard lk(_mutex);
            _suspend = true;
        }
    }

    void PeriodicTimer::resume() {
        {
            std::lock_guard lk(_mutex);
            _suspend = false;
        }
    }

    void PeriodicTimer::run() {
        std::unique_lock lk(_mutex); 
        while (!_stop) {
            auto now = std::chrono::system_clock::now();
            std::cv_status status;
            auto deadline = now + _period;
            do {
                status = _cond.wait_until(lk, deadline);
            } while (!_stop && status != std::cv_status::timeout);
            if (!_stop && !_suspend) {
                _dispatcher.push(_callback);
            }
        }
    }
}