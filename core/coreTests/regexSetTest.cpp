#include "../RegexSet.h"

#include "gtest/gtest.h"
#include "gtest/gtest.h"

namespace
{
	using namespace YAM;

	TEST(RegexSet, constructEmptySet) {
		RegexSet set;
		EXPECT_TRUE(set.regexStrings().empty());
		EXPECT_FALSE(set.matches(""));
	}

	TEST(RegexSet, constructMatchAllSet) {
		RegexSet set({ (".*") });
		EXPECT_EQ(1, set.regexStrings().size());
		EXPECT_TRUE(set.matches(""));
	}

	TEST(RegexSet, add) {
		RegexSet set;
		set.add("peter$");
		set.add("^peter");
		EXPECT_TRUE(set.matches("dit is peter"));
		EXPECT_TRUE(set.matches("peter dit is"));
		EXPECT_FALSE(set.matches("is peter dit"));
	}

	TEST(RegexSet, remove) {
		RegexSet set;
		set.add("peter$");
		set.add("^peter");
		set.remove("peter$");
		EXPECT_FALSE(set.matches("dit is peter"));
		EXPECT_TRUE(set.matches("peter dit is"));
		EXPECT_FALSE(set.matches("is peter dit"));
	}

	TEST(RegexSet, clear) {
		RegexSet set;
		set.add("peter$");
		set.add("^peter");
		set.clear();
		EXPECT_FALSE(set.matches("dit is peter"));
		EXPECT_FALSE(set.matches("peter dit is"));
		EXPECT_FALSE(set.matches("is peter dit"));
	}

	TEST(RegexSet, matchFileSuffix) {
		RegexSet set;
		set.add("_special.cpp$");
		EXPECT_TRUE(set.matches("aap\\.cpp\\mies\\source_special.cpp"));
		EXPECT_FALSE(set.matches("aap\\.cpp\\mies\\source_special.c"));
	}
}