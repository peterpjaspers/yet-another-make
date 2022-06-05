#include "gtest/gtest.h"
#include "executeNode.h"
#include "../FileNode.h"
#include "../ExecutionContext.h"
#include "../../xxhash/xxhash.h"

#include <chrono>
#include <cstdio>

namespace
{
    using namespace YAM;

    std::chrono::seconds timeout(10);

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
}

namespace
{
    using namespace YAM;

    TEST(FileNode, executeFileExists) {
        std::string content("Hello world");
        auto testPath(std::filesystem::temp_directory_path() / "fileNode_tmp.txt");
        XXH64_hash_t expectedHash = hashString(content);
        EXPECT_TRUE(createTestFile(testPath, content));

        ExecutionContext context;
        FileNode fnode(&context, testPath);
        EXPECT_NE(expectedHash, fnode.executionHash());
        EXPECT_EQ(Node::State::Dirty, fnode.state());

        bool completed = YAMTest::executeNode(&fnode);

        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, fnode.state());
        EXPECT_EQ(expectedHash, fnode.executionHash());
        EXPECT_EQ(expectedHash, fnode.computeExecutionHash());

        std::filesystem::remove(testPath);
    }

    TEST(FileNode, executeFileDeleted) {
        std::string content("Hello world");
        auto testPath(std::filesystem::temp_directory_path() / "fileNode_tmp.txt");
        XXH64_hash_t expectedHash = hashString(content);

        ExecutionContext context;
        FileNode fnode(&context, testPath);
        EXPECT_NE(expectedHash, fnode.executionHash());
        EXPECT_EQ(Node::State::Dirty, fnode.state());

        bool completed = YAMTest::executeNode(&fnode);

        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, fnode.state());
        EXPECT_NE(expectedHash, fnode.executionHash());
        EXPECT_NE(expectedHash, fnode.computeExecutionHash());

    }
}
