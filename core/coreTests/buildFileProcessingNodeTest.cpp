
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../BuildFileProcessingNode.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
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

        TestSetup()
            : repoTree(FileSystem::createUniqueDirectory("_buildFileProcessingTest"), 2, RegexSet())
            , fileRepo(std::make_shared<FileRepository>("repo", repoTree.path(), &context))
            , absBuildFilePath(repoTree.path() / R"(buildfile_yam.bat)")
        {
            std::string echoOff("@echo off\n");
            std::string buildFileProgramText(R"(echo : foreach *.cpp ^|^< cc %f -o %o ^|^< %B.obj)");
            std::ofstream stream(absBuildFilePath.string().c_str());
            EXPECT_TRUE(stream.is_open());
            stream << echoOff << std::endl;
            stream << buildFileProgramText << std::endl;
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
    }
}