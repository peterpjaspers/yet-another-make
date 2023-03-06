#pragma once

#include "IStreamable.h"

#include <chrono>
#include <string>
#include <vector>

namespace YAM
{
    class IStreamer;

    struct __declspec(dllexport) WallClockTime
    {
    public:
        // Construct with all fields set to their minimum value.
        WallClockTime();

        // Construct from vector: year = args[0], month = args[1], etc.
        // When args contains less than 7 elements the missing fields 
        // are initialized to their minimum value.
        WallClockTime(std::vector<uint32_t> args);

        // Construct time from dateTime string yyyy-mm-dd hh:mm:ss.uuuuuu
        WallClockTime(std::string dateTime);

        uint16_t year() const { return _year; }
        uint16_t month() const { return _month; }
        uint16_t day() const { return _day; }
        uint16_t hour() const { return _hour; }
        uint16_t minute() const { return _minute; }
        uint16_t second() const { return _second; }
        uint32_t usecond() const { return _usecond; }

        void year(uint16_t);
        void month(uint16_t);
        void day(uint16_t);
        void hour(uint16_t);
        void minute(uint16_t);
        void second(uint16_t);
        void usecond(uint32_t);

        // return yyyy-mm-dd hh:mm:ss.uuuuuu
        std::string dateTime() const;

        // return yyyy-mm-dd
        std::string date() const;

        // return hh:mm:ss.uuuuuu
        std::string time6() const;

        // return hh:mm:ss.uuu
        std::string time3() const;

        // return hh:mm:ss.uu
        std::string time2() const;

        // return hh:mm:ss.u
        std::string time1() const;

        void stream(IStreamer* streamer);

    private:
        uint16_t _year;   // 1900...
        uint16_t _month;  // 1..12
        uint16_t _day;    // 1..31
        uint16_t _hour;   // 0..23
        uint16_t _minute; // 0..59
        uint16_t _second; // 0..59
        uint32_t _usecond;  // 0..999999
    };
}

namespace YAM
{
    class __declspec(dllexport) TimePoint
    {
    public:
        TimePoint(std::chrono::system_clock::time_point time = std::chrono::system_clock::now());
        TimePoint(WallClockTime const& wctime);

        std::chrono::system_clock::time_point const& time() const;
        WallClockTime const& wctime() const;

        void stream(IStreamer* streamer);

    private:
        std::chrono::system_clock::time_point _time;
        WallClockTime _wctime;
    };
}
