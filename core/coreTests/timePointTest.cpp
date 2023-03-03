#include "../TimePoint.h"

#include "gtest/gtest.h"

namespace
{
	using namespace YAM;

	WallClockTime wct( std::vector<uint32_t>{ 2023, 2, 14, 11, 01, 10, 698765 } );

	TEST(TimePoint, construct) {
		TimePoint tp(wct);
		std::chrono::system_clock::time_point const& time = tp.time();
		TimePoint actualTp(time);
		WallClockTime actualWct = actualTp.wctime();
		EXPECT_EQ(wct.year, actualWct.year);
		EXPECT_EQ(wct.month, actualWct.month);
		EXPECT_EQ(wct.day, actualWct.day);
		EXPECT_EQ(wct.hour, actualWct.hour);
		EXPECT_EQ(wct.minute, actualWct.minute);
		EXPECT_EQ(wct.second, actualWct.second);
		EXPECT_EQ(wct.usecond, actualWct.usecond);
	}

	TEST(WallClockTime, construct) {
		WallClockTime t(wct.dateTime());
		EXPECT_EQ("2023-02-14 11:01:10.698765", t.dateTime());
	}

	TEST(WallClockTime, dateTime) {
		std::string dt = wct.dateTime();
		EXPECT_EQ("2023-02-14 11:01:10.698765", dt);
	}

	TEST(WallClockTime, time6) {
		std::string dt = wct.time6();
		EXPECT_EQ("11:01:10.698765", dt);
	}

	TEST(WallClockTime, time3) {
		std::string dt = wct.time3();
		EXPECT_EQ("11:01:10.699", dt);
	}

	TEST(WallClockTime, time2) {
		std::string dt = wct.time2();
		EXPECT_EQ("11:01:10.70", dt);
	}

	TEST(WallClockTime, time1) {
		std::string dt = wct.time1();
		EXPECT_EQ("11:01:10.7", dt);
	}
}