#include "../DotYamDirectory.h"
#include "../FileSystem.h"
#include "../MemoryLogBook.h"

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

    TEST(DotYamDirectory, initializeInGitRepo) {
        MemoryLogBook logBook;
        std::filesystem::path repoDir = FileSystem::createUniqueDirectory();
        std::filesystem::create_directory(repoDir / ".git");
        std::filesystem::path subDir = repoDir / "sub";
        std::filesystem::create_directory(subDir);

        std::filesystem::path yamDir = DotYamDirectory::initialize(subDir, &logBook);
        std::filesystem::path expectedYamDir(repoDir / ".yam");
        EXPECT_EQ(expectedYamDir, yamDir);
        EXPECT_TRUE(std::filesystem::exists(yamDir));

        std::filesystem::remove_all(repoDir);
    }

    TEST(DotYamDirectory, failInitializeInGitRepo) {
        MemoryLogBook logBook;
        std::filesystem::path repoDir = FileSystem::createUniqueDirectory();
        std::filesystem::create_directory(repoDir / ".git");
        std::filesystem::path illegalYamDir = repoDir / "sub" / ".yam";
        std::filesystem::create_directory(illegalYamDir.parent_path());
        std::filesystem::create_directory(illegalYamDir);

        std::filesystem::path yamDir = DotYamDirectory::initialize(illegalYamDir.parent_path(), &logBook);
        std::filesystem::path expectedYamDir; // empty because illegal yam dir
        EXPECT_EQ(expectedYamDir, yamDir);
        EXPECT_EQ(1, logBook.records().size());
        EXPECT_EQ(LogRecord::Aspect::Error, logBook.records()[0].aspect);

        std::filesystem::remove_all(repoDir);
    }
}