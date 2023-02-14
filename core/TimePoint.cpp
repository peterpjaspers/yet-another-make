#include "TimePoint.h"

#include <sstream>
#include <ctime>
#include <iomanip>

namespace
{
    using namespace YAM;

    WallClockTime tp2wc(std::chrono::system_clock::time_point const& tp)
    {
        const time_t tt = std::chrono::system_clock::to_time_t(tp);
        struct tm tm;
        localtime_s(&tm, &tt);

        WallClockTime wctime;
        wctime.year = tm.tm_year + 1900;
        wctime.month = tm.tm_mon + 1;
        wctime.day = tm.tm_mday;
        wctime.hour = tm.tm_hour;
        wctime.minute = tm.tm_min;
        wctime.second = tm.tm_sec;
        std::chrono::microseconds usecs = 
            std::chrono::duration_cast<std::chrono::microseconds>(
                tp - std::chrono::system_clock::from_time_t(tt));
        wctime.usecond = static_cast<unsigned int>(usecs.count());
        return wctime;
    }

    std::chrono::system_clock::time_point wc2tp(WallClockTime const& wctime)
    {
        std::tm tm = {};
        tm.tm_isdst = -1;
        tm.tm_year = wctime.year - 1900;
        tm.tm_mon = wctime.month - 1;
        tm.tm_mday = wctime.day;
        tm.tm_hour = wctime.hour;
        tm.tm_min = wctime.minute;
        tm.tm_sec = wctime.second;

        std::chrono::system_clock::time_point tp = 
            std::chrono::system_clock::from_time_t(std::mktime(&tm));
        tp += std::chrono::microseconds(wctime.usecond);
        return tp;
    }
}

namespace YAM
{
    std::string WallClockTime::dateTime() const {
        std::stringstream ss;
        ss << year << '-';
        if (month < 10) ss << '0';
        ss << month << '-';
        if (day < 10) ss << '0';
        ss << day << ' ';
        if (hour <= 9) ss << '0';
        ss << hour << ':';
        if (minute < 10) ss << '0';
        ss << minute << ':';
        if (second < 10) ss << '0';
        ss << second << '.';
        ss << std::setfill('0') << std::setw(6) << usecond;
        return ss.str();
    }

    std::string WallClockTime::date() const {
        std::stringstream ss;
        ss << year << '-';
        ss << month << '-';
        if (day <= 9) ss << '0';
        ss << day;
        return ss.str();
    }

    std::string WallClockTime::time6() const {
        std::stringstream ss;
        if (hour <= 9) ss << '0';
        ss << hour << ':';
        if (minute <= 9) ss << '0';
        ss << minute << ':';
        if (second < 10) ss << '0';
        ss << second << '.';
        ss << std::setfill('0') << std::setw(6) << usecond;
        return ss.str();
    }

    std::string WallClockTime::time3() const {
        std::stringstream ss;
        if (hour <= 9) ss << '0';
        ss << hour << ':';
        if (minute <= 9) ss << '0';
        ss << minute << ':';
        if (second < 10) ss << '0';
        ss << second << '.';
        ss << std::setfill('0') << std::setw(3) << (int)((usecond/1000.0) + 1.0);
        return ss.str();
    }

    std::string WallClockTime::time2() const {
        std::stringstream ss;
        if (hour <= 9) ss << '0';
        ss << hour << ':';
        if (minute <= 9) ss << '0';
        ss << minute << ':';
        if (second < 10) ss << '0';
        ss << second << '.';
        ss << std::setfill('0') << std::setw(2) << (int)((usecond / 10000.0) + 1.0);
        return ss.str();
    }

    std::string WallClockTime::time1() const {
        std::stringstream ss;
        if (hour <= 9) ss << '0';
        ss << hour << ':';
        if (minute <= 9) ss << '0';
        ss << minute << ':';
        if (second < 10) ss << '0';
        ss << second << '.';
        ss << (int)((usecond / 100000.0) + 1.0);
        return ss.str();
    }

    TimePoint::TimePoint(std::chrono::system_clock::time_point time)
        : _time(time)
        , _wctime(tp2wc(time))
    {}

    TimePoint::TimePoint(WallClockTime const& wctime)
        : _time(wc2tp(wctime))
        , _wctime(wctime)
    {}

    std::chrono::system_clock::time_point const& TimePoint::time() const {
        return _time;
    }

    WallClockTime const& TimePoint::wctime() const {
        return _wctime;
    }
}


