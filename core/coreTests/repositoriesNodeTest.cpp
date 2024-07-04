#include "executeNode.h"
#include "../RepositoriesNode.h"
#include "../DirectoryNode.h"
#include "../FileSystem.h"
#include "../FileRepositoryNode.h"
#include "../ExecutionContext.h"
#include "../MemoryLogBook.h"

#include "gtest/gtest.h"
#include <memory>
#include <fstream>

namespace
{
    using namespace YAM;

    void writeFile(std::filesystem::path const& p, std::string const& content) {
        std::ofstream stream(p.string().c_str());
        stream << content;
        stream.close();
    }

    class TestSetup
    {
    public:
        TestSetup()
            : homeRepoDir(FileSystem::createUniqueDirectory("_repositoriesNodeTest"))
            , repositoriesPath(homeRepoDir / RepositoriesNode::configFilePath())
            , logBook(std::make_shared<MemoryLogBook>())
            , fileRepo(std::make_shared<FileRepositoryNode>(&context, ".", homeRepoDir))
        {
            context.logBook(logBook);
            auto repos = std::make_shared<RepositoriesNode>(&context, fileRepo);
            repos->ignoreConfigFile(false);
            context.repositoriesNode(repos);
            repositoriesNode = context.repositoriesNode();

            std::filesystem::create_directory(homeRepoDir / "yamConfig");            
            EXPECT_EQ(repositoriesPath, repositoriesNode->absoluteConfigFilePath());
        }

        ~TestSetup() {
            context.clearBuildState();
            std::filesystem::remove_all(homeRepoDir);
        }

        std::filesystem::path homeRepoDir;
        std::filesystem::path repositoriesPath;
        ExecutionContext context;
        std::shared_ptr<MemoryLogBook> logBook;
        std::shared_ptr<FileRepositoryNode> fileRepo;
        std::shared_ptr<RepositoriesNode> repositoriesNode;
    };

    TEST(RepositoriesNode, parse) {
        TestSetup setup;

        std::filesystem::create_directory(setup.homeRepoDir.parent_path() / "r1");
        std::filesystem::create_directory(setup.homeRepoDir.parent_path() / "r2");
        std::stringstream ss;
        ss << "name=repo1 dir=" << "..\\r1" << " type=Build ;" << std::endl;
        ss << "name=repo2 dir=" << "..\\r2" << " type=Track ;" << std::endl;
        writeFile(setup.repositoriesPath, ss.str());

        bool completed = YAMTest::executeNode(setup.repositoriesNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, setup.repositoriesNode->state());
        EXPECT_NE(nullptr, setup.context.findRepository("."));
        auto frepo1 = setup.context.findRepository("repo1");
        EXPECT_NE(nullptr, frepo1);
        EXPECT_EQ(setup.homeRepoDir.parent_path() / "r1", frepo1->directoryNode()->absolutePath());
        auto frepo2 = setup.context.findRepository("repo2");
        EXPECT_NE(nullptr, frepo2);
        EXPECT_EQ(setup.homeRepoDir.parent_path() / "r2", frepo2->directoryNode()->absolutePath());

        std::filesystem::remove(setup.homeRepoDir.parent_path() / "r1");
        std::filesystem::remove(setup.homeRepoDir.parent_path() / "r2");
    }

    TEST(RepositoriesNode, invalidRepoName) {
        TestSetup setup;
        std::stringstream ss;
        ss << "name=repo/1 dir=" << "../sub" << " type=Build ;" << std::endl;
        writeFile(setup.repositoriesPath, ss.str());
        bool completed = YAMTest::executeNode(setup.repositoriesNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Failed, setup.repositoriesNode->state());
        LogRecord const& r = setup.logBook->records()[0];
        std::string msg("Unexpected token at line 1");
        EXPECT_NE(std::string::npos, r.message.find(msg));
    }

    TEST(RepositoriesNode, invalidRepoDir) {
        TestSetup setup;
        std::stringstream ss;
        ss << "name=repo dir=" << "D:/aap" << " type=Build ;" << std::endl;
        writeFile(setup.repositoriesPath, ss.str());
        bool completed = YAMTest::executeNode(setup.repositoriesNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Failed, setup.repositoriesNode->state());
        LogRecord const& r = setup.logBook->records()[0];
        std::string msg("Repository directory D:/aap does not exist");
        EXPECT_NE(std::string::npos, r.message.find(msg));
    }

    TEST(RepositoriesNode, invalidRepoType) {
        TestSetup setup;
        std::filesystem::create_directory(setup.homeRepoDir.parent_path() / "sub1");
        std::stringstream ss;
        ss << "name=repo2 dir=" << "..\\sub1" << " type=blabla ;" << std::endl;
        writeFile(setup.repositoriesPath, ss.str());
        bool completed = YAMTest::executeNode(setup.repositoriesNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Failed, setup.repositoriesNode->state());
        LogRecord const& r = setup.logBook->records()[0];
        std::string msg("Must be one of Build, Track or Ignore");
        EXPECT_NE(std::string::npos, r.message.find(msg));
        std::filesystem::remove(setup.homeRepoDir / "sub1");
    }

    TEST(RepositoriesNode, invalidRepoDirParent) {
        TestSetup setup;
        std::stringstream ss;
        ss << "name=repo1 dir=" << ".." << " type=Build ;" << std::endl;
        writeFile(setup.repositoriesPath, ss.str());
        bool completed = YAMTest::executeNode(setup.repositoriesNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Failed, setup.repositoriesNode->state());
        LogRecord const& r = setup.logBook->records()[0];
        std::string msg("repository directory is parent directory of repository \".\" ");
        EXPECT_NE(std::string::npos, r.message.find(msg));
    }

    TEST(RepositoriesNode, invalidRepoDirSub) {
        TestSetup setup;
        std::stringstream ss;
        std::filesystem::create_directory(setup.homeRepoDir / "sub");
        ss << "name=repo1 dir=" << ".\\sub" << " type = Build; " << std::endl;
        writeFile(setup.repositoriesPath, ss.str());
        bool completed = YAMTest::executeNode(setup.repositoriesNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Failed, setup.repositoriesNode->state());
        LogRecord const& r = setup.logBook->records()[0];
        std::string msg("repository directory is sub-directory of repository \".\"");
        EXPECT_NE(std::string::npos, r.message.find(msg));
        std::filesystem::remove(setup.homeRepoDir / "sub");
    }

    TEST(RepositoriesNode, invalidRepoDirEqual) {
        TestSetup setup;
        std::stringstream ss;
        std::filesystem::create_directories(setup.homeRepoDir.parent_path() / "r1");
        ss << "name=repo1 dir=" << "../r1" << " type=Build ;" << std::endl;
        ss << "name=repo2 dir=" << "../r1" << " type=Build ;" << std::endl;
        writeFile(setup.repositoriesPath, ss.str());
        bool completed = YAMTest::executeNode(setup.repositoriesNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Failed, setup.repositoriesNode->state());
        LogRecord const& r = setup.logBook->records()[0];
        std::string msg("repository directory is equal to directory of repository \"repo1\"");
        EXPECT_NE(std::string::npos, r.message.find(msg));
        std::filesystem::remove(setup.homeRepoDir.parent_path() / "r1");
    }
}