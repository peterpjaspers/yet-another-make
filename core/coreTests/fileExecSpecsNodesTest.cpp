#include "executeNode.h"
#include "../FileExecSpecsNode.h"
#include "../FileSystem.h"
#include "../FileRepositoryNode.h"
#include "../ExecutionContext.h"
#include "../MemoryLogBook.h"
#include "../RepositoriesNode.h"

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
            : repoDir(FileSystem::createUniqueDirectory("_fileExecSpecsTest"))
            , logBook(std::make_shared<MemoryLogBook>())
            , fileRepo(std::make_shared<FileRepositoryNode>(&context, "repo", repoDir, FileRepositoryNode::RepoType::Build))
            , fileExecSpecsNode(fileRepo->fileExecSpecsNode())
        {
            context.logBook(logBook);
            std::filesystem::create_directory(repoDir / "yamConfig");
            std::filesystem::path fileExecSpecsPath(repoDir / FileExecSpecsNode::configFilePath());
            std::stringstream ss;
            if (syntaxError) {
                ss << ".bat => cmd.exe /c %f" << std::endl;
                ss << ".cmd cmd.exe /c %f" << std::endl;
            } else {
                ss << ".bat => cmd.exe /c %f" << std::endl;
                ss << ".cmd => cmd.exe /c %f" << std::endl;
                ss << ".py => py.exe %f" << std::endl;
                ss << ".exe => %f" << std::endl;
                ss << ".fun => This is %%f %f" << std::endl;
            }
            writeFile(fileExecSpecsPath, ss.str());
            auto repos = std::make_shared<RepositoriesNode>(&context, fileRepo);
            context.repositoriesNode(repos);
        }

        ~TestSetup() {
            context.repositoriesNode()->removeRepository(fileRepo->repoName());
            std::filesystem::remove_all(repoDir);
        }

        std::filesystem::path repoDir;
        ExecutionContext context;
        std::shared_ptr<MemoryLogBook> logBook;
        std::shared_ptr<FileRepositoryNode> fileRepo;
        std::shared_ptr<FileExecSpecsNode> fileExecSpecsNode;
    };

    TEST(FileExecSpecsNode, parse) {
        TestSetup setup(false);
        bool completed = YAMTest::executeNode(setup.fileExecSpecsNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, setup.fileExecSpecsNode->state());
        EXPECT_EQ("cmd.exe /c buildfile.bat", setup.fileExecSpecsNode->command("buildfile.bat"));
        EXPECT_EQ("cmd.exe /c buildfile.cmd", setup.fileExecSpecsNode->command("buildfile.cmd"));
        EXPECT_EQ("py.exe buildfile.py", setup.fileExecSpecsNode->command("buildfile.py"));
        EXPECT_EQ("buildfile.exe", setup.fileExecSpecsNode->command("buildfile.exe"));
        EXPECT_EQ("This is %%f buildfile.fun", setup.fileExecSpecsNode->command("buildfile.fun"));
    }

    TEST(FileExecSpecsNode, parseError) {
        TestSetup setup(true);
        bool completed = YAMTest::executeNode(setup.fileExecSpecsNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Failed, setup.fileExecSpecsNode->state());
        LogRecord const& r = setup.logBook->records()[0];
        std::string msg("Unexpected token at line 2, column 6 in file ");        
        msg.append(setup.fileExecSpecsNode->absoluteConfigFilePath().string());
        msg.append("\n");
        EXPECT_EQ(msg, r.message);
    }
    
    TEST(FileExecSpecsNode, notFound) {
        TestSetup setup(false);
        bool completed = YAMTest::executeNode(setup.fileExecSpecsNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, setup.fileExecSpecsNode->state());
        EXPECT_EQ("", setup.fileExecSpecsNode->command("buildfile.txt"));
    }
}