#include "../BuildOptionsParser.h"

#include "gtest/gtest.h"

#include <string>

namespace
{
    using namespace YAM;

    TEST(BuildOptionsParser, noOptions) {
        char program[] = "yam";
        char* argv[] = { program };
        BuildOptions options;
        BuildOptionsParser parser(1, argv, options);
        EXPECT_FALSE(parser.parseError());
        EXPECT_FALSE(options._clean);
        EXPECT_TRUE(options._scope.empty());
        EXPECT_EQ(2, options._logAspects.size());
    }

    TEST(BuildOptionsParser, clean) {
        char program[] = "yam";
        char clean[] = "--clean";
        char* argv[] = { program, clean };
        BuildOptions options;
        BuildOptionsParser parser(2, argv, options);
        EXPECT_FALSE(parser.parseError());
        EXPECT_TRUE(options._clean);
    }

    TEST(BuildOptionsParser, files) {
        char program[] = "yam";
        char noopt[] = "--";
        char f1[] = "file1";
        char f2[] = "file2";
        char* argv[] = { program, noopt, f1, f2 };
        BuildOptions options;
        BuildOptionsParser parser(4, argv, options);
        EXPECT_FALSE(parser.parseError());
        EXPECT_FALSE(options._clean);
        EXPECT_EQ(2, options._scope.size());
        auto const& scope = options._scope;
        EXPECT_NE(scope.end(), std::find(scope.begin(), scope.end(), f1));
        EXPECT_NE(scope.end(), std::find(scope.begin(), scope.end(), f2));
    }
}
