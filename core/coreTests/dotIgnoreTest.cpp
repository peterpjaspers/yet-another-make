#include "../FileSystem.h"
#include "../Glob.h"
#include "../DotIgnoreParser.h"
#include "DirectoryTree.h"

#include "gtest/gtest.h"

using namespace YAM;

namespace 
{
    using namespace YAMTest;

    std::filesystem::path tmpDir = FileSystem::createUniqueDirectory();
    RegexSet excludes;
    DirectoryTree testTree(tmpDir / "_dotIgnoreTest", 1, excludes);
    std::filesystem::path baseDir = testTree.path();
    std::string source = ".ignore, line 2"; // fake source

    TEST(DotIgnoreParser, read) {
        DotIgnoreParser parser(R"(C:\Users\peter\Documents\yam\github\main\.gitignore)");
    }
    TEST(DotIgnoreRule, IgnoreNotAnchored) {
        DotIgnoreRule rule("*.py[cod]", source);
        EXPECT_TRUE(rule.ignore(".pyc"));
        EXPECT_TRUE(rule.ignore("main.pyc"));
        EXPECT_TRUE(rule.ignore("/main.pyc"));
        EXPECT_TRUE(rule.ignore("dir/main.pyc"));
        EXPECT_TRUE(rule.ignore("/dir/main.pyc"));

        DotIgnoreRule rrule("*.py[c-o]", source);
        EXPECT_TRUE(rrule.ignore(".pyc"));
        EXPECT_TRUE(rrule.ignore("main.pyc"));
        EXPECT_TRUE(rrule.ignore("/main.pyc"));
        EXPECT_TRUE(rrule.ignore("dir/main.pyc"));
        EXPECT_TRUE(rrule.ignore("/dir/main.pyc"));
    }

    TEST(DotIgnoreRule, NotIgnoreNotAnchored) {
        DotIgnoreRule rule("*.py[cod]", source);
        EXPECT_FALSE(rule.ignore("main.pc"));
        EXPECT_FALSE(rule.ignore("dir/main.pc"));
        EXPECT_FALSE(rule.ignore("/dir/main.pc"));
        EXPECT_FALSE(rule.ignore("pc"));

        DotIgnoreRule rrule("*.py[c-o]", source);
        EXPECT_FALSE(rrule.ignore("main.pc"));
        EXPECT_FALSE(rrule.ignore("dir/main.pc"));
        EXPECT_FALSE(rrule.ignore("/dir/main.pc"));
        EXPECT_FALSE(rrule.ignore("pc"));

        DotIgnoreRule nrule("!*.py[cod]", source);
        EXPECT_FALSE(rule.ignore("main.pc"));
        EXPECT_FALSE(rule.ignore("dir/main.pc"));
        EXPECT_FALSE(rule.ignore("/dir/main.pc"));
        EXPECT_FALSE(rule.ignore("pc"));
        EXPECT_FALSE(nrule.ignore(".pyc"));
        EXPECT_FALSE(nrule.ignore("main.pyc"));
        EXPECT_FALSE(nrule.ignore("/main.pyc"));
        EXPECT_FALSE(nrule.ignore("dir/main.pyc"));
        EXPECT_FALSE(nrule.ignore("/dir/main.pyc"));
    }

    TEST(DotIgnoreRule, IgnoreAnchored) {
        DotIgnoreRule rule("/di?/*.py[cod]", source);
        EXPECT_TRUE(rule.ignore("dir/main.pyc"));
        EXPECT_TRUE(rule.ignore("/dir/main.pyc"));
        EXPECT_TRUE(rule.ignore("dip/main.pyc"));
        EXPECT_TRUE(rule.ignore("/dip/main.pyc"));
    }

    TEST(DotIgnoreRule, NotIgnoreAnchored) {
        DotIgnoreRule rule("/di?/*.py[cod]", source);
        EXPECT_FALSE(rule.ignore("main.pyc"));
        EXPECT_FALSE(rule.ignore("/main.pyc"));
        EXPECT_FALSE(rule.ignore("dir/main.pc"));
        EXPECT_FALSE(rule.ignore("/dir/main.pc"));
        EXPECT_FALSE(rule.ignore("sub/dir/main.pyc"));
        EXPECT_FALSE(rule.ignore("/sub/dir/main.pyc"));

        DotIgnoreRule nrule("!/di?/*.py[cod]", source);
        EXPECT_FALSE(nrule.ignore("main.pyc"));
        EXPECT_FALSE(nrule.ignore("/main.pyc"));
        EXPECT_FALSE(nrule.ignore("dir/main.pc"));
        EXPECT_FALSE(nrule.ignore("/dir/main.pc"));
        EXPECT_FALSE(nrule.ignore("sub/dir/main.pyc"));
        EXPECT_FALSE(nrule.ignore("/sub/dir/main.pyc"));
        EXPECT_FALSE(nrule.ignore("dir/main.pyc"));
        EXPECT_FALSE(nrule.ignore("/dir/main.pyc"));
        EXPECT_FALSE(nrule.ignore("dip/main.pyc"));
        EXPECT_FALSE(nrule.ignore("/dip/main.pyc"));
    }
}