#include "gtest/gtest.h"
#include "executeNode.h"
#include "../FileNode.h"
#include "../FileSystem.h"
#include "../ExecutionContext.h"
#include "../FileRepositoryNode.h"
#include "../RepositoriesNode.h"
#include "../xxhash.h"

#include <chrono>
#include <fstream>

namespace
{
    using namespace YAM;

    bool createTestFile(std::filesystem::path testPath, std::string const &content) {
        std::ofstream stream(testPath);
        stream << content;
        bool ok = stream.good();
        stream.close();
        return ok;
    }

    XXH64_hash_t hashString(std::string const& content) {
        return XXH64(content.c_str(), content.length(), 0);
    }

    std::string entireFile = FileAspect::entireFileAspect().name();

    class Driver {
    public:
        std::filesystem::path repoDir;
        ExecutionContext context;
        std::shared_ptr<FileRepositoryNode> repo;

        Driver()
            : repoDir(FileSystem::createUniqueDirectory())
            , repo(std::make_shared<FileRepositoryNode>(
                &context,
                ".",
                repoDir))
        {
            auto repos = std::make_shared<RepositoriesNode>(&context, repo);
            context.repositoriesNode(repos);
        }

        ~Driver() {
            context.repositoriesNode()->removeRepository(repo->repoName());
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

        auto fnode = std::make_shared<FileNode>(&driver.context, driver.repo->symbolicPathOf(testPath));
        driver.context.nodes().add(fnode);
        EXPECT_EQ(testPath.string(), fnode->absolutePath().string());
        EXPECT_EQ(Node::State::Dirty, fnode->state());
        EXPECT_ANY_THROW(fnode->hashOf(entireFile));

        bool completed = YAMTest::executeNode(fnode.get());

        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, fnode->state());
        EXPECT_EQ(expectedHash, fnode->hashOf(entireFile));
    }

    TEST(FileNode, executeFileDeleted) {
        Driver driver;

        std::string content("Hello world");
        XXH64_hash_t expectedHash = hashString(content);

        auto testPath(driver.repoDir / "fileNode_del.txt");
        auto fnode = std::make_shared<FileNode>(&driver.context, driver.repo->symbolicPathOf(testPath));
        driver.context.nodes().add(fnode);
        EXPECT_EQ(testPath.string(), fnode->absolutePath().string());
        EXPECT_EQ(Node::State::Dirty, fnode->state());
        EXPECT_ANY_THROW(fnode->hashOf(entireFile));

        bool completed = YAMTest::executeNode(fnode.get());

        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, fnode->state());
        EXPECT_NE(expectedHash, fnode->hashOf(entireFile));
    }

    std::chrono::time_point<std::filesystem::_File_time_clock> lastWriteTime(std::filesystem::path const& path) {
        std::error_code ec;
        auto lwt = std::filesystem::last_write_time(path, ec);
        return lwt;
    }

    std::chrono::time_point<std::chrono::utc_clock> lastWriteTimeUtc(std::filesystem::path const& path) {
        auto lwt = lastWriteTime(path);
        auto lwutc = decltype(lwt)::clock::to_utc(lwt);
        return lwutc;
    }

    bool appendToFile(std::filesystem::path path, std::string const& content) {
        std::ofstream stream(path, std::ios_base::app);
        stream << content;
        bool ok = stream.good();
        stream.close();
        return ok;
    }

    TEST(FileNode, lastWriteTimeResolution) {
        Driver driver;
        std::filesystem::path testFile(driver.repoDir / "text.txt");

        std::vector<long long> deltas;
        appendToFile(testFile, "");
        auto t0 = lastWriteTime(testFile);
        for (int i = 0; i < 10000; ++i) {
            appendToFile(testFile, "a");
            auto t = lastWriteTime(testFile);
            auto delta = t - t0;
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count();
            deltas.push_back(ns);
            t0 = t;
        }
    }
}
