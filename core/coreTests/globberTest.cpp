#include "../FileRepositoryNode.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../Globber.h"
#include "../RepositoriesNode.h"

#include "gtest/gtest.h"
#include "executeNode.h"
#include "DirectoryTree.h"

namespace
{
    using namespace YAM;
    using namespace YAMTest;

    // Create directory tree, mirror tree in source directory node and
    // store resulting directory node tree to repoDir/ "nodes"
    class GlobberSetup
    {
    public:

        std::string repoName;
        std::filesystem::path repoDir;
        DirectoryTree testTree;
        ExecutionContext context;

        GlobberSetup()
            : repoName("repo")
            , repoDir(FileSystem::createUniqueDirectory())
            , testTree(repoDir, 3, RegexSet({ }))
        {
            //context.threadPool().size(1);
            auto homeRepo = std::make_shared<FileRepositoryNode>(
                &context,
                "repo",
                repoDir); 
            auto repos = std::make_shared<RepositoriesNode>(&context, homeRepo);
            context.repositoriesNode(repos);

            bool completed = YAMTest::executeNode(rootDir().get());
            EXPECT_TRUE(completed);
        }

        std::shared_ptr<FileRepositoryNode> repo() {
            return context.findRepository(repoName);
        }

        std::shared_ptr<DirectoryNode> rootDir() {
            return repo()->directoryNode();
        }
    };

    TEST(Globber, File1) {
        GlobberSetup setup;
        std::filesystem::path pattern("File1");
        std::filesystem::path expectedAbsPath(setup.repoDir / pattern);
        std::filesystem::path expectedSymPath = setup.repo()->symbolicPathOf(expectedAbsPath);

        Globber globber(setup.rootDir(), pattern, false);
        auto const& matches = globber.matches();
        auto const& inputDirs = globber.inputDirs();
        ASSERT_EQ(1, matches.size());
        EXPECT_EQ(expectedSymPath, matches[0]->name());
        ASSERT_EQ(1, inputDirs.size());
        EXPECT_TRUE(inputDirs.contains(setup.rootDir()));
    }

    TEST(Globber, SubDir1File3) {
        GlobberSetup setup;
        std::filesystem::path pattern(R"(SubDir1\File3)");
        std::filesystem::path expectedAbsPath(setup.repoDir / pattern);
        std::filesystem::path expectedSymPath = setup.repo()->symbolicPathOf(expectedAbsPath);

        Globber globber(setup.rootDir(), pattern, false);
        auto const& matches = globber.matches();
        auto const& inputDirs = globber.inputDirs();
        ASSERT_EQ(1, matches.size());
        EXPECT_EQ(expectedSymPath, matches[0]->name());
        ASSERT_EQ(1, inputDirs.size());
        auto inputDir = *inputDirs.begin();
        EXPECT_EQ(setup.rootDir()->name() / pattern.parent_path(), inputDir->name());
    }

    TEST(Globber, SubDir1SubDir2) {
        GlobberSetup setup;
        std::filesystem::path pattern(R"(SubDir1\SubDir2)");
        std::filesystem::path expectedAbsPath(setup.repoDir / pattern);
        std::filesystem::path expectedPath = setup.repo()->symbolicDirectory() / pattern;

        Globber globber(setup.rootDir(), pattern, false);
        auto const& matches = globber.matches();
        auto const& inputDirs = globber.inputDirs();
        ASSERT_EQ(1, matches.size());
        EXPECT_EQ(expectedPath, matches[0]->name());
        ASSERT_EQ(1, inputDirs.size());
        auto inputDir = *inputDirs.begin();
        EXPECT_EQ(setup.rootDir()->name() / pattern, inputDir->name());
    }

    TEST(Globber, AllFilesInRoot) {
        GlobberSetup setup;
        std::filesystem::path pattern("..\\File[123]");
        std::filesystem::path expected1 = setup.repo()->symbolicDirectory() / "File1";
        std::filesystem::path expected2 = setup.repo()->symbolicDirectory() / "File2";
        std::filesystem::path expected3 = setup.repo()->symbolicDirectory() / "File3";

        std::vector<std::shared_ptr<DirectoryNode>> subDirs;
        setup.rootDir()->getSubDirs(subDirs);
        auto subDir = subDirs[0];
        Globber globber(subDir, pattern, false);
        auto const& matches = globber.matches();
        auto const& inputDirs = globber.inputDirs();
        ASSERT_EQ(3, matches.size());
        EXPECT_EQ(expected1, matches[0]->name());
        EXPECT_EQ(expected2, matches[1]->name());
        EXPECT_EQ(expected3, matches[2]->name());
        ASSERT_EQ(1, inputDirs.size());
        auto inputDir = *inputDirs.begin();
        EXPECT_EQ(setup.rootDir(), inputDir);
    }

    TEST(Globber, AllFiles12) {
        GlobberSetup setup;
        std::filesystem::path pattern(R"(**\File[12])");

        Globber globber(setup.rootDir(), pattern, false);
        auto const& matches = globber.matches();
        auto const& inputDirs = globber.inputDirs();
        ASSERT_EQ(80, matches.size());
        EXPECT_EQ(40, inputDirs.size());
    }

    TEST(Globber, AllFiles12WithSymbolicPathPattern) {
        GlobberSetup setup;
        std::filesystem::path pattern(setup.rootDir()->name() / R"(**\File[12])");

        Globber globber(setup.rootDir(), pattern, false);
        auto const& matches = globber.matches();
        auto const& inputDirs = globber.inputDirs();
        ASSERT_EQ(80, matches.size());
        EXPECT_EQ(40, inputDirs.size());
    }

    TEST(Globber, AllFilesAndDirs) {
        GlobberSetup setup;
        std::filesystem::path pattern(R"(**)");

        Globber globber(setup.rootDir(), pattern, false);
        auto const& matches = globber.matches();
        auto const& inputDirs = globber.inputDirs();
        ASSERT_EQ(160, matches.size());
        EXPECT_EQ(40, inputDirs.size());
    }

    TEST(Globber, AllDirs) {
        GlobberSetup setup;
        std::filesystem::path pattern(R"(**\)");

        Globber globber(setup.rootDir(), pattern, false);
        auto const& matches = globber.matches();
        auto const& inputDirs = globber.inputDirs();
        ASSERT_EQ(40, matches.size());
        EXPECT_EQ(40, inputDirs.size());
    }

    TEST(Globber, AllDirsWithSymbolicPathPattern) {
        GlobberSetup setup;
        std::filesystem::path pattern = setup.rootDir()->name() / (R"(**\)");

        Globber globber(setup.rootDir(), pattern, false);
        auto const& matches = globber.matches();
        auto const& inputDirs = globber.inputDirs();
        ASSERT_EQ(40, matches.size());
        EXPECT_EQ(40, globber.inputDirs().size());
    }
}