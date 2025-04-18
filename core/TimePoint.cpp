#include "TimePoint.h"
#include "IStreamer.h"

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
        wctime.year(tm.tm_year + 1900);
        wctime.month(tm.tm_mon + 1);
        wctime.day(tm.tm_mday);
        wctime.hour(tm.tm_hour);
        wctime.minute(tm.tm_min);
        wctime.second(tm.tm_sec);
        std::chrono::microseconds usecs = 
            std::chrono::duration_cast<std::chrono::microseconds>(
                tp - std::chrono::system_clock::from_time_t(tt));
        wctime.usecond(static_cast<unsigned int>(usecs.count()));
        return wctime;
    }

    std::chrono::system_clock::time_point wc2tp(WallClockTime const& wctime)
    {
        std::tm tm = {};
        tm.tm_isdst = -1;
        tm.tm_year = wctime.year() - 1900;
        tm.tm_mon  = wctime.month() - 1;
        tm.tm_mday = wctime.day();
        tm.tm_hour = wctime.hour();
        tm.tm_min  = wctime.minute();
        tm.tm_sec  = wctime.second();

        std::chrono::system_clock::time_point tp = 
            std::chrono::system_clock::from_time_t(std::mktime(&tm));
        tp += std::chrono::microseconds(wctime.usecond());
        return tp;
    }
}

namespace YAM
{
    WallClockTime::WallClockTime()
        : _year(1900), _month(1), _day(1)
        , _hour(0), _minute(0), _second(0), _usecond(0)
    {}

    WallClockTime::WallClockTime(std::vector<uint32_t> args) : WallClockTime() {
        if (args.size() > 0) year(static_cast<uint16_t>(args[0]));
        if (args.size() > 1) month(static_cast<uint16_t>(args[1]));
        if (args.size() > 2) day(static_cast<uint16_t>(args[2]));
        if (args.size() > 3) hour(static_cast<uint16_t>(args[3]));
        if (args.size() > 4) minute(static_cast<uint16_t>(args[4]));
        if (args.size() > 5) second(static_cast<uint16_t>(args[5]));
        if (args.size() > 6) usecond(args[6]);
    }

    // yyyy-mm-dd hh:mm:ss.uuuuuu
    WallClockTime::WallClockTime(std::string dateTime) {
        std::stringstream ss(dateTime);
        char delim;
        uint16_t y, mo, d, h, m, s;
        uint32_t us;
        ss
            >> y >> delim >> mo >> delim >> d
            >> h >> delim >> m >> delim >> s >> delim >> us;
        year(y);
        month(mo);
        day(d);
        hour(h);
        minute(m);
        second(s);
        usecond(us);
    }

    void WallClockTime::year(uint16_t year) {
        if (year < 1900) throw std::exception("year out of range");
        _year = year;
    }
    void WallClockTime::month(uint16_t month) {
        if (month < 1 || month > 12) throw std::exception("month out of range");
        _month = month;
    }
    void WallClockTime::day(uint16_t day) {
        if (day < 1 || day > 31) throw std::exception("day out of range");
        _day = day;
    }
    void WallClockTime::hour(uint16_t hour) {
        if (hour > 23) throw std::exception("hour out of range");
        _hour = hour;
    }
    void WallClockTime::minute(uint16_t minute) {
        if (minute > 59) throw std::exception("minute out of range");
        _minute = minute;
    }
    void WallClockTime::second(uint16_t second) {
        if (second > 59) throw std::exception("second of range");
        _second = second;
    }
    void WallClockTime::usecond(uint32_t usecond) {
        if (usecond > 999999) throw std::exception("usecond out of range");
        _usecond = usecond;
    }

    std::string WallClockTime::dateTime() const {
        std::stringstream ss;
        ss << _year << '-';
        if (_month < 10) ss << '0';
        ss << _month << '-';
        if (_day < 10) ss << '0';
        ss << _day << ' ';
        if (_hour <= 9) ss << '0';
        ss << _hour << ':';
        if (_minute < 10) ss << '0';
        ss << _minute << ':';
        if (_second < 10) ss << '0';
        ss << _second << '.';
        ss << std::setfill('0') << std::setw(6) << _usecond;
        return ss.str();
    }

    std::string WallClockTime::date() const {
        std::stringstream ss;
        ss << _year << '-';
        ss << _month << '-';
        if (_day <= 9) ss << '0';
        ss << _day;
        return ss.str();
    }

    std::string WallClockTime::time6() const {
        std::stringstream ss;
        if (_hour <= 9) ss << '0';
        ss << _hour << ':';
        if (_minute <= 9) ss << '0';
        ss << _minute << ':';
        if (_second < 10) ss << '0';
        ss << _second << '.';
        ss << std::setfill('0') << std::setw(6) << _usecond;
        return ss.str();
    }

    std::string WallClockTime::time3() const {
        std::stringstream ss;
        if (_hour <= 9) ss << '0';
        ss << _hour << ':';
        if (_minute <= 9) ss << '0';
        ss << _minute << ':';
        if (_second < 10) ss << '0';
        ss << _second << '.';
        ss << std::setfill('0') << std::setw(3) << (int)((_usecond/1000.0) + 1.0);
        return ss.str();
    }

    std::string WallClockTime::time2() const {
        std::stringstream ss;
        if (_hour <= 9) ss << '0';
        ss << _hour << ':';
        if (_minute <= 9) ss << '0';
        ss << _minute << ':';
        if (_second < 10) ss << '0';
        ss << _second << '.';
        ss << std::setfill('0') << std::setw(2) << (int)((_usecond / 10000.0) + 1.0);
        return ss.str();
    }

    std::string WallClockTime::time1() const {
        std::stringstream ss;
        if (_hour <= 9) ss << '0';
        ss << _hour << ':';
        if (_minute <= 9) ss << '0';
        ss << _minute << ':';
        if (_second < 10) ss << '0';
        ss << _second << '.';
        ss << (int)((_usecond / 100000.0) + 1.0);
        return ss.str();
    }

    void WallClockTime::stream(IStreamer* streamer) {
        std::string tps;
        if (streamer->writing()) tps = dateTime();
        streamer->stream(tps);
        if (streamer->reading()) *this = WallClockTime(tps);
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

    void TimePoint::stream(IStreamer* streamer) {
        _wctime.stream(streamer);
        if (streamer->reading()) {
            _time = wc2tp(_wctime);
        }
    }

    TimeDuration::TimeDuration(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end
    )
        : TimeDuration(end - start)
    {
    }

    TimeDuration::TimeDuration(std::chrono::system_clock::duration duration)
        : _duration(duration)
    {
    }

    std::string TimeDuration::toString(std::chrono::system_clock::duration const& duration) {
        auto milliSeconds = (std::chrono::duration_cast<std::chrono::milliseconds>(duration)).count();

        long long hours = milliSeconds / (60 * 60 * 1000);
        milliSeconds -= hours * 60 * 60 * 1000;
        long long minutes = milliSeconds / (60 * 1000);
        milliSeconds -= minutes * 60 * 1000;
        long long seconds = milliSeconds / 1000;
        milliSeconds -= seconds * 1000;

        std::stringstream ss;
        if (hours == 1) {
            ss << "1 hour ";
        }
        else if (hours > 1) {
            ss << hours << " hours ";
        }
        if (minutes == 1) {
            ss << "1 minute ";
        }
        else if (minutes > 1) {
            ss << minutes << " minutes ";
        }
        if (seconds == 1) {
            ss << "1 second ";
        }
        else if (seconds > 1) {
            ss << seconds << " seconds ";
        }
        if (milliSeconds == 1) {
            ss << "1 milliseconds ";
        }
        else if (milliSeconds > 1) {
            ss << milliSeconds << " milliseconds";
        }
        return ss.str();
    }
}


