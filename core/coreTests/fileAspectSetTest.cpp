#include "../FileAspectSet.h"

#include "gtest/gtest.h"

namespace
{
	using namespace YAM;

	TEST(FileAspectSet, construct) {
		FileAspectSet set;
		EXPECT_TRUE(set.aspects().empty());
		EXPECT_FALSE(set.find("entireFile").first);
		EXPECT_FALSE(set.find("cpp-code").first);
		EXPECT_EQ(FileAspect::entireFileAspect().name(), set.findApplicableAspect("source.cpp").name());
	}

	TEST(FileAspectSet, add) {
		FileAspectSet set;
		FileAspect aspect("cpp-code", RegexSet({ "\\.cpp$" }));
		set.add(aspect);
		EXPECT_EQ(1, set.aspects().size());
		EXPECT_TRUE(set.find("cpp-code").first);
		EXPECT_EQ(aspect.name(), set.findApplicableAspect("source.cpp").name());
	}

	TEST(FileAspectSet, remove) {
		FileAspectSet set;
		FileAspect aspect1("cpp-code", RegexSet({ "\\.cpp$" }));
		FileAspect aspect2("c-code", RegexSet({ "\\.c$" }));
		set.add(aspect1);
		EXPECT_EQ(1, set.aspects().size());
		set.remove(aspect2);
		EXPECT_EQ(1, set.aspects().size());
		set.remove(aspect1);
		EXPECT_EQ(0, set.aspects().size());
	}

	TEST(FileAspectSet, clear) {
		FileAspectSet set;
		FileAspect aspect1("cpp-code", RegexSet({ "\\.cpp$" }));
		FileAspect aspect2("c-code", RegexSet({ "\\.c$" }));
		set.clear();
		EXPECT_EQ(0, set.aspects().size());
	}

	TEST(FileAspectSet, aspects) {
		FileAspectSet set;
		FileAspect aspect1("cpp-code", RegexSet({ "\\.cpp$" }));
		FileAspect aspect2("c-code", RegexSet({ "\\.c$" }));
		set.add(aspect1);
		set.add(aspect2);
		EXPECT_EQ(aspect1.name(), set.aspects()[0].name());
		EXPECT_EQ(aspect2.name(), set.aspects()[1].name());
	}

	TEST(FileAspectSet, find) {
		FileAspectSet set;
		FileAspect aspect1("cpp-code", RegexSet({ "\\.cpp$" }));
		FileAspect aspect2("c-code", RegexSet({ "\\.c$" }));
		set.add(aspect1);
		set.add(aspect2);
		EXPECT_EQ(aspect1.name(), set.find(aspect1.name()).second.name());
		EXPECT_EQ(aspect2.name(), set.find(aspect2.name()).second.name());
	}

	TEST(FileAspectSet, findApplicableAspect) {
		FileAspectSet set;
		FileAspect aspect1("cpp-code", RegexSet({ "\\.cpp$" }));
		FileAspect aspect2("c-code", RegexSet({ "\\.c$" }));
		set.add(aspect1);
		set.add(aspect2);
		EXPECT_EQ(aspect1.name(), set.findApplicableAspect("source.cpp").name());
		EXPECT_EQ(aspect2.name(), set.findApplicableAspect("source.c").name());
		EXPECT_EQ(FileAspect::entireFileAspect().name(), set.findApplicableAspect("source.cs").name());
	}
}