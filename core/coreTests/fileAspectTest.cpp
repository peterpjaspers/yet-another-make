#include "../FileAspect.h"

#include "gtest/gtest.h"

namespace
{
	using namespace YAM;

	TEST(FileAspect, construct) {
		FileAspect aspect("cpp-code", RegexSet({ "\\.cpp$" , "\\.c$", "\\.h$" }));
		EXPECT_EQ("cpp-code", aspect.name());
		RegexSet patterns = aspect.fileNamePatterns();
		EXPECT_EQ(3, patterns.regexStrings().size());
		EXPECT_EQ("\\.cpp$", patterns.regexStrings()[0]);
		EXPECT_EQ("\\.c$", patterns.regexStrings()[1]);
		EXPECT_EQ("\\.h$", patterns.regexStrings()[2]);
	}

	TEST(FileAspect, matches) {
		FileAspect aspect("cpp-code", RegexSet({ "\\.cpp$" , "\\.c$", "\\.h$" }));
		EXPECT_TRUE(aspect.matches("source.cpp"));
		EXPECT_TRUE(aspect.matches("source.c"));
		EXPECT_TRUE(aspect.matches("source.h"));
		EXPECT_FALSE(aspect.matches("source.cs"));
	}

	TEST(FileAspect, entireFileAspect) {
		FileAspect aspect = FileAspect::entireFileAspect();
		EXPECT_EQ("entireFile", aspect.name());
		RegexSet patterns = aspect.fileNamePatterns();
		EXPECT_EQ(1, patterns.regexStrings().size());
		EXPECT_EQ(".*", patterns.regexStrings()[0]);
	}
}