#include "../RegexSet.h"

#include "gtest/gtest.h"

namespace
{
    using namespace YAM;

    TEST(RegexSet, constructEmptySet) {
        RegexSet set;
        EXPECT_TRUE(set.regexStrings().empty());
        EXPECT_FALSE(set.matches(""));
    }

    TEST(RegexSet, constructNonEmptySet) {
        RegexSet set({ (".*") });
        EXPECT_EQ(1, set.regexStrings().size());
        EXPECT_TRUE(set.matches(""));
        EXPECT_TRUE(set.matches("dit is peter"));
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
        set.add("\\.c$");
        EXPECT_TRUE(set.matches("aap\\.cpp\\mies\\source_special.cpp"));
        EXPECT_FALSE(set.matches("aap\\.cpp\\mies\\source.cpp"));
        EXPECT_FALSE(set.matches("aap\\.cpp\\mies\\source.cppc"));
        EXPECT_TRUE(set.matches("aap\\.cpp\\mies\\source.c"));
    }

    TEST(RegexSet, matchDirectory) {
        RegexSet set({ RegexSet::matchDirectory(std::string("generated")) });
        EXPECT_TRUE(set.matches(std::string("C:\\repo\\module\\generated\\file.obj")));
        EXPECT_TRUE(set.matches(std::string("/repo/module/generated/file.obj")));
        EXPECT_TRUE(set.matches(std::string("/repo/module/generated")));
        EXPECT_TRUE(set.matches(std::string("/repo/module/generated/")));
        EXPECT_FALSE(set.matches(std::string("/repo/module/generated ")));
    }
}