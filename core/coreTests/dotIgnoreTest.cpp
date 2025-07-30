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

    // Debug: 239 us/rule
    // Release:
    //     regex only:  7 us/rule
    //     ext + regex: 3 us/rule
    // 
    TEST(DotIgnoreParser, read) {
        DotIgnoreParser parser(R"(..\..\.gitignore)");
        bool ignored = false;
        std::filesystem::path p("core/x64/Release/yamServer.exe");
        int nRules = 0;
        int nIter = 100;
        auto start = std::chrono::system_clock::now();
        for (int i = 0; i < nIter; i++) {
            for (auto const& rule : parser.rules()) {
                nRules += 1;
                if (rule.ignore(p)) ignored = true;
            }
        }
        ASSERT_TRUE(ignored);
        auto elapsed = std::chrono::system_clock::now() - start;
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
        auto usCnt = us.count();
        auto usPerRule = usCnt /nRules;
        std::cout << usPerRule << " us/rule" << std::endl;
    }

    // Debug: 7557 us/rule (!?)
    // Release: 16 us/rule
    //TEST(DotIgnoreParser, readMerged) {
    void noread() {
        DotIgnoreParser parser(R"(..\..\.gitignore)");
        bool ignored = false;
        std::filesystem::path p("core/x64/Release/yamServer.exe");
        auto const& rules = parser.rules();
        int nIter = 10;
        int nRules = rules.size();
        auto pstr = p.string();
        std::stringstream mergedRe;
        mergedRe << "(?:";
        for (int i = 0; i < nRules - 1; i++) {
            mergedRe << rules[i].reString() << ")|(?:";
        }
        mergedRe << rules[nRules - 1].reString() << ")";
        auto mergedReStr = mergedRe.str();
        std::regex re(mergedReStr, std::regex::optimize);
        auto start = std::chrono::system_clock::now();
        for (int i = 0; i < nIter; i++) {
            std::smatch re_match;
            bool matches = std::regex_match(pstr, re_match, re);
            if (matches) ignored = true;
        }
        ASSERT_TRUE(ignored);
        auto elapsed = std::chrono::system_clock::now() - start;
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
        auto usCnt = us.count();
        auto usPerRule = usCnt / (nRules*nIter);
        std::cout << usPerRule << " us/rule" << std::endl;
    }

    TEST(DotIgnoreRule, IgnoreNotAnchored) {
        DotIgnoreRule rule("*.py[cod]", source);
        EXPECT_TRUE(rule.ignore(".pyc"));
        EXPECT_TRUE(rule.ignore("main.pyc"));
        EXPECT_TRUE(rule.ignore("/main.pyc"));
        EXPECT_TRUE(rule.ignore("dir/main.pyc"));
        EXPECT_TRUE(rule.ignore("/dir/main.pyc"));
        EXPECT_TRUE(rule.ignore("/dir/main.pyc"));

        DotIgnoreRule rrule("*.py[c-o]", source);
        EXPECT_TRUE(rrule.ignore(".pyc"));
        EXPECT_TRUE(rrule.ignore("main.pyc"));
        EXPECT_TRUE(rrule.ignore("/main.pyc"));
        EXPECT_TRUE(rrule.ignore("dir/main.pyc"));
        EXPECT_TRUE(rrule.ignore("/dir/main.pyc"));

        DotIgnoreRule drule("*.py?/", source);
        EXPECT_TRUE(drule.ignore(".pyc/"));
        EXPECT_TRUE(drule.ignore("main.pyc/"));
        EXPECT_TRUE(drule.ignore("/main.pyc/"));
        EXPECT_TRUE(drule.ignore("dir/main.pyc/"));
        EXPECT_TRUE(drule.ignore("/dir/main.pyc/"));

        DotIgnoreRule lrule("main.pyc", source);
        EXPECT_TRUE(lrule.ignore("main.pyc"));
        EXPECT_TRUE(lrule.ignore("dir/main.pyc"));

        DotIgnoreRule tdrule("/core/coreTests/testData/", source);
        EXPECT_TRUE(tdrule.ignore("core/coreTests/testData/"));
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

        DotIgnoreRule drule("*.py?/", source);
        EXPECT_FALSE(drule.ignore(".pyc"));
        EXPECT_FALSE(drule.ignore("main.pyc"));
        EXPECT_FALSE(drule.ignore("/main.pyc"));
        EXPECT_FALSE(drule.ignore("dir/main.pyc"));
        EXPECT_FALSE(drule.ignore("/dir/main.pyc"));

        DotIgnoreRule lrule("main.pyc", source);
        EXPECT_FALSE(lrule.ignore("main.pyc/"));
    }

    TEST(DotIgnoreRule, IgnoreAnchored) {
        DotIgnoreRule rule1("/di?/*.py[cod]", source);
        EXPECT_TRUE(rule1.ignore("dir/main.pyc"));
        EXPECT_TRUE(rule1.ignore("/dir/main.pyc"));
        EXPECT_TRUE(rule1.ignore("dip/main.pyc"));
        EXPECT_TRUE(rule1.ignore("/dip/main.pyc"));

        DotIgnoreRule rule2("di?/*.py[cod]", source);
        EXPECT_TRUE(rule2.ignore("dir/main.pyc"));
        EXPECT_TRUE(rule2.ignore("/dir/main.pyc"));
        EXPECT_TRUE(rule2.ignore("dip/main.pyc"));
        EXPECT_TRUE(rule2.ignore("/dip/main.pyc"));

        DotIgnoreRule rule3("/detours/**", source);
        EXPECT_TRUE(rule3.ignore("detours/inc/detours.h"));

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