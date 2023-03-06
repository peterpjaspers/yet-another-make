#include "../FileAspect.h"

#include "gtest/gtest.h"

namespace
{
    using namespace YAM;

    Delegate<XXH64_hash_t, std::filesystem::path const&> const& entireFileHasher = FileAspect::entireFileAspect().hashFunction();

    std::string testString("/*dit is een fileaspect hasher test string*/");
    std::filesystem::path testPath(std::filesystem::temp_directory_path() / "fileHasherTest.cpp");

    bool createTestFile(std::string const& content) {
        FILE* fp = std::fopen(testPath.string().c_str(), "w");
        if (!fp) return false;

        auto nWritten = std::fwrite(content.c_str(), sizeof(char), content.length(), fp);
        if (nWritten != content.length()) return false;

        std::fclose(fp);
        return true;
    }

    XXH64_hash_t hashString(std::string const& content) {
        return XXH64(content.c_str(), content.length(), 0);
    }
    TEST(FileAspect, construct) {
        FileAspect aspect("cpp-code", RegexSet({ "\\.cpp$" , "\\.c$", "\\.h$" }), entireFileHasher);
        EXPECT_EQ("cpp-code", aspect.name());
        RegexSet patterns = aspect.fileNamePatterns();
        EXPECT_EQ(3, patterns.regexStrings().size());
        EXPECT_EQ("\\.cpp$", patterns.regexStrings()[0]);
        EXPECT_EQ("\\.c$", patterns.regexStrings()[1]);
        EXPECT_EQ("\\.h$", patterns.regexStrings()[2]);
    }

    TEST(FileAspect, appliesTo) {
        FileAspect aspect("cpp-code", RegexSet({ "\\.cpp$" , "\\.c$", "\\.h$" }), entireFileHasher);
        EXPECT_TRUE(aspect.appliesTo("source.cpp"));
        EXPECT_TRUE(aspect.appliesTo("source.c"));
        EXPECT_TRUE(aspect.appliesTo("source.h"));
        EXPECT_FALSE(aspect.appliesTo("source.cs"));
    }

    TEST(FileAspect, entireFileAspect) {
        FileAspect aspect = FileAspect::entireFileAspect();
        EXPECT_EQ("entireFile", aspect.name());
        RegexSet patterns = aspect.fileNamePatterns();
        EXPECT_EQ(1, patterns.regexStrings().size());
        EXPECT_EQ(".*", patterns.regexStrings()[0]);
    }

    TEST(FileAspect, hash) {

        FileAspect aspect("cpp-code", RegexSet({ "\\.cpp$" , "\\.c$", "\\.h$" }), entireFileHasher);

        EXPECT_TRUE(aspect.appliesTo(testPath));
        EXPECT_TRUE(createTestFile(testString));
        XXH64_hash_t expectedHash = hashString(testString);
        EXPECT_EQ(expectedHash, aspect.hash(testPath));
        std::filesystem::remove(testPath);
    }
}