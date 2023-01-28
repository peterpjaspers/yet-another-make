#include "gtest/gtest.h"
#include "executeNode.h"
#include "../FileNode.h"
#include "../ExecutionContext.h"
#include "../SourceFileRepository.h"
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
}

namespace
{
    using namespace YAM;

    TEST(FileNode, executeFileExists) {
        std::string content("Hello world");
        auto testPath(std::filesystem::temp_directory_path() / "fileNode_ex.txt");
        XXH64_hash_t expectedHash = hashString(content);
        EXPECT_TRUE(createTestFile(testPath, content));

        ExecutionContext context;
        FileNode fnode(&context, testPath);
        EXPECT_EQ(Node::State::Dirty, fnode.state());
        EXPECT_ANY_THROW(fnode.hashOf(entireFile));

        bool completed = YAMTest::executeNode(&fnode);

        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, fnode.state());
        EXPECT_EQ(expectedHash, fnode.hashOf(entireFile));

        std::filesystem::remove(testPath);
    }

    TEST(FileNode, executeFileDeleted) {
        std::string content("Hello world");
        auto testPath(std::filesystem::temp_directory_path() / "fileNode_del.txt");
        XXH64_hash_t expectedHash = hashString(content);

        ExecutionContext context;
        FileNode fnode(&context, testPath);
        EXPECT_EQ(Node::State::Dirty, fnode.state());
        EXPECT_ANY_THROW(fnode.hashOf(entireFile));

        bool completed = YAMTest::executeNode(&fnode);

        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, fnode.state());
        EXPECT_NE(expectedHash, fnode.hashOf(entireFile));
    }

    TEST(FileNode, relativePath) {

        std::filesystem::path repoDir(std::tmpnam(nullptr));
        std::filesystem::create_directories(repoDir);
        ExecutionContext context;
        auto repo = std::make_shared<SourceFileRepository>(
            std::string("repo"), 
            repoDir,
            RegexSet(),
            &context);
        context.addRepository(repo);

        std::filesystem::path expectedRelativePath("sources\\file.cpp");
        FileNode fnode(&context, repoDir / expectedRelativePath);
        EXPECT_EQ(expectedRelativePath, fnode.relativePath());

        context.removeRepository(repo->name());
        repo.reset();
        std::filesystem::remove_all(repoDir);
    }
}
