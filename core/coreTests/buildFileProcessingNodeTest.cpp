
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../BuildFileProcessingNode.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../CommandNode.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../RegexSet.h"

#include "gtest/gtest.h"
#include <chrono>

namespace
{
    using namespace YAM;
    using namespace YAMTest;

    class TestSetup {
    public:
        DirectoryTree repoTree;
        ExecutionContext context;
        std::shared_ptr<FileRepository> fileRepo;
        std::filesystem::path absBuildFilePath;
        std::filesystem::path cmdOutputFile;

        TestSetup()
            : repoTree(FileSystem::createUniqueDirectory("_buildFileProcessingTest"), 2, RegexSet())
            , fileRepo(std::make_shared<FileRepository>("repo", repoTree.path(), &context))
            , absBuildFilePath(repoTree.path() / R"(buildfile_yam.bat)")
            , cmdOutputFile(fileRepo->directoryNode()->name() / "main.obj")
        {
            //R"(echo : foreach *.cpp ^|^< cc %f -o %o ^|^< %B.obj)"

            std::filesystem::create_directory(repoTree.path() / "outputs");
            std::ofstream stream(absBuildFilePath.string().c_str());
            EXPECT_TRUE(stream.is_open());
            stream
                << "@echo off" << std::endl
                << R"(echo : main.cpp ^|^> cc main.cpp -o main.obj ^|^> main.obj)"
                << std::endl;
            stream.close();

            context.addRepository(fileRepo);
            auto dirNode = fileRepo->directoryNode();
            bool completed = YAMTest::executeNode(dirNode.get());
            EXPECT_TRUE(completed);
        }

        std::shared_ptr<BuildFileProcessingNode> processingNode() {
            auto nodeName = fileRepo->directoryNode()->name() / "__buildfile";
            auto node = context.nodes().find(nodeName);
            return dynamic_pointer_cast<BuildFileProcessingNode>(node);
        }
    };

    TEST(BuildFileProcessingNode, executeProcessingNode) {
        TestSetup setup;
        std::shared_ptr<BuildFileProcessingNode> processingNode = setup.processingNode();
        ASSERT_NE(nullptr, processingNode);
        EXPECT_EQ(Node::State::Dirty, processingNode->state());
        bool completed = YAMTest::executeNode(processingNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, processingNode->state());
        std::filesystem::path cmdName = setup.fileRepo->symbolicDirectory() / "main.obj\\__cmd";
        std::shared_ptr<Node> node = setup.context.nodes().find(cmdName);
        ASSERT_NE(nullptr, node);
        std::shared_ptr<CommandNode> cmdNode = dynamic_pointer_cast<CommandNode>(node);
        ASSERT_NE(nullptr, cmdNode);
        EXPECT_EQ(Node::State::Dirty, cmdNode->state());

        completed = YAMTest::executeNode(cmdNode.get());
        EXPECT_TRUE(completed);
    }
}