#include "executeNode.h"
#include "../RepositoriesNode.h"
#include "../DirectoryNode.h"
#include "../FileSystem.h"
#include "../FileRepository.h"
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
        TestSetup(bool syntaxError)
            : homeRepoDir(FileSystem::createUniqueDirectory("_repositoriesNodeTest"))
            , repo1Dir(FileSystem::createUniqueDirectory("_repositoriesNodeTest"))
            , repo2Dir(FileSystem::createUniqueDirectory("_repositoriesNodeTest"))
            , logBook(std::make_shared<MemoryLogBook>())
            , fileRepo(std::make_shared<FileRepository>(".", homeRepoDir, &context, false))
        {
            context.logBook(logBook);
            auto repos = std::make_shared<RepositoriesNode>(&context, fileRepo);
            repos->ignoreConfigFile(false);
            context.repositoriesNode(repos);
            repositoriesNode = context.repositoriesNode();

            std::filesystem::create_directory(homeRepoDir / "yamConfig");
            std::filesystem::path repositoriesPath(homeRepoDir / RepositoriesNode::configFilePath());
            std::stringstream ss;
            if (syntaxError) {
                ss << "name=repo1 dir=" << repo1Dir << " types=Integrated ;" << std::endl;
                ss << "name=repo2 dir=" << repo2Dir << " type=Tracked ;" << std::endl;

            } else {
                ss << "name=repo1 dir=" << repo1Dir << " type=Integrated ;" << std::endl;
                ss << "name=repo2 dir=" << repo2Dir << " type=Tracked ;" << std::endl;
            }
            writeFile(repositoriesPath, ss.str()); 
            
            EXPECT_EQ(repositoriesPath, repositoriesNode->absoluteConfigFilePath());
        }

        ~TestSetup() {
            context.clearBuildState();
            std::filesystem::remove_all(homeRepoDir);
            std::filesystem::remove_all(repo1Dir);
            std::filesystem::remove_all(repo2Dir);
        }

        std::filesystem::path homeRepoDir;
        std::filesystem::path repo1Dir;
        std::filesystem::path repo2Dir;
        ExecutionContext context;
        std::shared_ptr<MemoryLogBook> logBook;
        std::shared_ptr<FileRepository> fileRepo;
        std::shared_ptr<RepositoriesNode> repositoriesNode;
    };

    TEST(RepositoriesNode, parse) {
        TestSetup setup(false);
        bool completed = YAMTest::executeNode(setup.repositoriesNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, setup.repositoriesNode->state());
        EXPECT_NE(nullptr, setup.context.findRepository("."));
        auto frepo1 = setup.context.findRepository("repo1");
        EXPECT_NE(nullptr, frepo1);
        EXPECT_EQ(setup.repo1Dir, frepo1->directoryNode()->absolutePath());
        auto frepo2 = setup.context.findRepository("repo2");
        EXPECT_NE(nullptr, frepo2);
        EXPECT_EQ(setup.repo2Dir, frepo2->directoryNode()->absolutePath());
    }

    TEST(RepositoriesNode, parseError) {
        TestSetup setup(true);
        bool completed = YAMTest::executeNode(setup.repositoriesNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Failed, setup.repositoriesNode->state());
        LogRecord const& r = setup.logBook->records()[0];
        std::string msg("Unexpected token at line 0");
        EXPECT_EQ(0, r.message.find(msg));
    }
}