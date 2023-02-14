#pragma once

#include <chrono>
#include <string>

namespace YAM
{
    struct __declspec(dllexport) WallClockTime
    {
        unsigned short year;   // 1900...
        unsigned short month;  // 1..12
        unsigned short day;    // 1..31
        unsigned short hour;   // 0..23
        unsigned short minute; // 0..59
        unsigned short second; // 0..59
        unsigned int usecond;  // 0..999999

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
    };

	class __declspec(dllexport) TimePoint
	{
	public:
        TimePoint(std::chrono::system_clock::time_point time = std::chrono::system_clock::now());
        TimePoint(WallClockTime const& wctime);

        std::chrono::system_clock::time_point const& time() const;
        WallClockTime const& wctime() const;

	private:
        std::chrono::system_clock::time_point _time;
        WallClockTime _wctime;
	};
}
