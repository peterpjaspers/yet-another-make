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
        EXPECT_EQ(wct.year(), actualWct.year());
        EXPECT_EQ(wct.month(), actualWct.month());
        EXPECT_EQ(wct.day(), actualWct.day());
        EXPECT_EQ(wct.hour(), actualWct.hour());
        EXPECT_EQ(wct.minute(), actualWct.minute());
        EXPECT_EQ(wct.second(), actualWct.second());
        EXPECT_EQ(wct.usecond(), actualWct.usecond());
    }

    TEST(WallClockTime, construct) {
        WallClockTime t(wct.dateTime());
        EXPECT_EQ("2023-02-14 11:01:10.698765", t.dateTime());
    }

    TEST(WallClockTime, constructIllegal) {
        EXPECT_ANY_THROW(WallClockTime t("0023-02-14 11:01:10.698765"));
        EXPECT_ANY_THROW(WallClockTime t("2023-55-14 11:01:10.698765"));
        EXPECT_ANY_THROW(WallClockTime t("2023-02-88 11:01:10.698765"));
        EXPECT_ANY_THROW(WallClockTime t("2023-02-14 25:01:10.698765"));
        EXPECT_ANY_THROW(WallClockTime t("2023-02-14 11:77:10.698765"));
        EXPECT_ANY_THROW(WallClockTime t("2023-02-14 11:01:99.698765"));
        EXPECT_ANY_THROW(WallClockTime t("2023-02-14 11:01:10.69876599"));
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

	TEST(TimeDuration, toString) {
        TimePoint start0(std::vector<uint32_t>{ 2023, 2, 14, 11, 01, 10, 698765 });
        TimePoint end0 (std::vector<uint32_t> { 2023, 2, 14, 11, 01, 10, 698765 });
        auto str = TimeDuration::toString(end0.time() - start0.time());
        EXPECT_EQ("", str);

        TimePoint start1(std::vector<uint32_t>{ 2023, 12, 31, 23, 59, 59, 999999 });
        TimePoint end1 (std::vector<uint32_t> { 2024,  1,  1,  0,  0,  0,      0 });
        str = TimeDuration::toString(end1.time() - start1.time());
        EXPECT_EQ("", str);

        TimePoint start2(std::vector<uint32_t>{ 2023, 10, 10, 10, 10, 10, 100000 });
        TimePoint end2 (std::vector<uint32_t> { 2023, 10, 10, 12, 12, 12, 120000 });
        str = TimeDuration::toString(end2.time() - start2.time());
        EXPECT_EQ("2 hours 2 minutes 2 seconds 20 milliseconds", str);
	}
}