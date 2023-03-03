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
        uint16_t year;   // 1900...
        uint16_t month;  // 1..12
        uint16_t day;    // 1..31
        uint16_t hour;   // 0..23
        uint16_t minute; // 0..59
        uint16_t second; // 0..59
        uint32_t usecond;  // 0..999999

        // Construct with all fields set to their minimum value.
        WallClockTime();

        // Construct from vector: year = args[0], month = args[1], etc.
        // When args contains less than 7 elements corresponding fields 
        // are initialzed to their minimum value.
        WallClockTime(std::vector<uint32_t> args);

        // Construct time from dateTime string yyyy-mm-dd hh:mm:ss.uuuuuu
        WallClockTime(std::string dateTime);

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
    };

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
