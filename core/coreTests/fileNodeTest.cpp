#include "gtest/gtest.h"
#include "executeNode.h"
#include "../FileNode.h"
#include "../FileRepository.h"
#include "../FileSystem.h"
#include "../ExecutionContext.h"
#include "../FileRepository.h"
#include "../../xxhash/xxhash.h"

#include <chrono>
#include <cstdio>

namespace
{
    using namespace YAM;

    bool createTestFile(std::filesystem::path testPath, std::string const &content) {
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

    std::string entireFile = FileAspect::entireFileAspect().name();

    class Driver {
    public:
        std::filesystem::path repoDir;
        ExecutionContext context;
        std::shared_ptr<FileRepository> repo;

        Driver()
            : repoDir(FileSystem::createUniqueDirectory())
            , repo(std::make_shared<FileRepository>(
                ".",
                repoDir,
                &context,
                true))
        {
            context.addRepository(repo);
        }

        ~Driver() {
            context.removeRepository(repo->name());
            repo = nullptr;
            std::filesystem::remove_all(repoDir);
        }
    };
}

namespace
{
    using namespace YAM;

    TEST(FileNode, executeFileExists) {
        Driver driver;

        std::string content("Hello world");
        auto testPath(driver.repoDir / "fileNode_ex.txt");
        XXH64_hash_t expectedHash = hashString(content);
        EXPECT_TRUE(createTestFile(testPath, content));

        FileNode fnode(&driver.context, driver.repo->symbolicPathOf(testPath));
        EXPECT_EQ(testPath.string(), fnode.absolutePath().string());
        EXPECT_EQ(Node::State::Dirty, fnode.state());
        EXPECT_ANY_THROW(fnode.hashOf(entireFile));

        bool completed = YAMTest::executeNode(&fnode);

        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, fnode.state());
        EXPECT_EQ(expectedHash, fnode.hashOf(entireFile));
    }

    TEST(FileNode, executeFileDeleted) {
        Driver driver;

        std::string content("Hello world");
        XXH64_hash_t expectedHash = hashString(content);

        auto testPath(driver.repoDir / "fileNode_del.txt");
        FileNode fnode(&driver.context, driver.repo->symbolicPathOf(testPath));
        EXPECT_EQ(testPath.string(), fnode.absolutePath().string());
        EXPECT_EQ(Node::State::Dirty, fnode.state());
        EXPECT_ANY_THROW(fnode.hashOf(entireFile));

        bool completed = YAMTest::executeNode(&fnode);

        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, fnode.state());
        EXPECT_NE(expectedHash, fnode.hashOf(entireFile));
    }
}
