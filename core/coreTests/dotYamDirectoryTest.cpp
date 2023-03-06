#include "../DotYamDirectory.h"
#include "../FileSystem.h"

#include "gtest/gtest.h"

namespace
{
    using namespace YAM;

    TEST(DotYamDirectory, create) {
        std::filesystem::path repoDir = FileSystem::createUniqueDirectory();
        std::filesystem::path yamDir = DotYamDirectory::create(repoDir);
        std::filesystem::path expected(repoDir / ".yam");
        EXPECT_EQ(expected, yamDir);
        EXPECT_TRUE(std::filesystem::exists(yamDir));
        std::filesystem::remove_all(repoDir);
    }

    TEST(DotYamDirectory, find) {
        std::filesystem::path repoDir = FileSystem::createUniqueDirectory();
        std::filesystem::path expected = DotYamDirectory::create(repoDir);
        std::filesystem::path actual = DotYamDirectory::find(repoDir);
        EXPECT_EQ(expected, actual);
        std::filesystem::remove_all(repoDir);
    }

    TEST(DotYamDirectory, findDeep) {
        std::filesystem::path repoDir = FileSystem::createUniqueDirectory();
        std::filesystem::path deepDir(repoDir / "sub" / "sub");
        std::filesystem::path expected = DotYamDirectory::create(repoDir);
        std::filesystem::path actual = DotYamDirectory::find(deepDir);
        EXPECT_EQ(expected, actual);
        std::filesystem::remove_all(repoDir);
    }

    TEST(DotYamDirectory, notFound) {
        std::filesystem::path repoDir = FileSystem::createUniqueDirectory();
        std::filesystem::path expected;
        std::filesystem::path actual = DotYamDirectory::find(repoDir);
        EXPECT_EQ(expected, actual);
        std::filesystem::remove_all(repoDir);
    }
    TEST(DotYamDirectory, notFoundDeep) {
        std::filesystem::path repoDir = FileSystem::createUniqueDirectory();
        std::filesystem::path deepDir(repoDir / "sub" / "sub");
        std::filesystem::path expected;
        std::filesystem::path actual = DotYamDirectory::find(deepDir);
        EXPECT_EQ(expected, actual);
        std::filesystem::remove_all(repoDir);
    }
}