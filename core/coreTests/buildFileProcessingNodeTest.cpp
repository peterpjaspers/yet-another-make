
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
        std::shared_ptr<SourceFileNode> mainFile;

        TestSetup()
            : repoTree(FileSystem::createUniqueDirectory("_buildFileProcessingTest"), 1, RegexSet())
            , fileRepo(std::make_shared<FileRepository>("repo", repoTree.path(), &context))
            , absBuildFilePath(repoTree.path() / R"(buildfile_yam.bat)")
            , cmdOutputFile(fileRepo->directoryNode()->name() / "main.obj")
        {
            std::ofstream stream(absBuildFilePath.string().c_str());
            EXPECT_TRUE(stream.is_open());
            stream
                << "@echo off" << std::endl
                << R"(echo : main.cpp ^|^> echo main ^> main.obj ^|^> main.obj)"
                << std::endl;
            stream.close();

            std::filesystem::path f1(repoTree.path() / "main.cpp");
            std::ofstream s1(f1.string().c_str());
            s1 << "void main() {}" << std::endl;
            s1.close();

            context.addRepository(fileRepo);
            auto dirNode = fileRepo->directoryNode();
            bool completed = YAMTest::executeNode(dirNode.get());
            EXPECT_TRUE(completed);
            mainFile = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(fileRepo->symbolicPathOf(f1)));
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
        EXPECT_EQ(Node::State::Ok, cmdNode->state());
        EXPECT_TRUE(std::filesystem::exists(setup.repoTree.path() / "main.obj"));
    }
}