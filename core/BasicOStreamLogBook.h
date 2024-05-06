#pragma once

#include "ILogBook.h"
#include <ostream>
#include <mutex>
#include <chrono>

namespace YAM
{
    class __declspec(dllexport) BasicOStreamLogBook : public ILogBook
    {
    public:
        BasicOStreamLogBook(std::basic_ostream<char, std::char_traits<char>> *ostream);

        // By default: log record.wctime.
        // When logging elapsed time: log (record.time.time() - _startTime)
        void logElapsedTime(bool enable) { _logElapsedTime = enable; }
        bool logElapsedTime() const { return _logElapsedTime; }

        void logAspect(bool enable) { _logAspect = enable; }
        bool logAspect() const { return _logAspect; }

        void add(LogRecord const& record) override;

    private:
        std::mutex _mutex;
        std::basic_ostream<char, std::char_traits<char>>* _ostream;
        std::chrono::system_clock::time_point _startTime;
        bool _logElapsedTime;
        bool _logAspect;
    };
}

