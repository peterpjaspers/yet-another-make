#include "gtest/gtest.h"
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../FileRepository.h"
#include "../SourceDirectoryNode.h"
#include "../FileNode.h"
#include "../ExecutionContext.h"
#include "../../xxhash/xxhash.h"

#include <chrono>

namespace
{
    using namespace YAM;
    using namespace YAMTest;

    std::chrono::seconds timeout(10);
    TEST(SourceDirectoryNode, construct_twoDeepDirectoryTree) {
        std::string tmpDir(std::tmpnam(nullptr));
        std::filesystem::path rootDir(std::string(tmpDir + "_dirNodeTest"));
        DirectoryTree testTree(rootDir, 2, RegexSet());

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        context.addRepository(std::make_shared<FileRepository>("repo", rootDir));
        auto dirNode = std::make_shared<SourceDirectoryNode>(&context, rootDir, nullptr);
        context.nodes().add(dirNode);
        dirNode->addPrerequisitesToContext();
        EXPECT_EQ(Node::State::Dirty, dirNode->state());

        bool completed = YAMTest::executeNode(dirNode.get());
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode.get());
    }

    TEST(SourceDirectoryNode, update_threeDeepDirectoryTree) {
        std::string tmpDir(std::tmpnam(nullptr));
        std::filesystem::path rootDir(std::string(tmpDir + "_dirNodeTest"));
        DirectoryTree testTree(rootDir, 3, RegexSet());

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        context.addRepository(std::make_shared<FileRepository>("repo", rootDir));
        auto dirNode = std::make_shared<SourceDirectoryNode>(&context, rootDir, nullptr);
        context.nodes().add(dirNode);
        dirNode->addPrerequisitesToContext();
        bool completed = YAMTest::executeNode(dirNode.get());
        EXPECT_TRUE(completed);

        // Update file system
        DirectoryTree* testTree_S1 = testTree.getSubDirs()[1];
        DirectoryTree* testTree_S1_S2 = testTree_S1->getSubDirs()[2];
        testTree.addFile();// adds 4-th file
        testTree_S1->addFile();// adds 4-th file
        testTree_S1_S2->addDirectory(); // adds 1 dir with 3 files and 3 subdirs
        testTree_S1_S2->addFile(); // adds 4-th file

        // Find nodes affected by file system changes...
        std::vector<std::shared_ptr<SourceDirectoryNode>> subDirNodes;
        dirNode->getSubDirs(subDirNodes);
        std::shared_ptr<SourceDirectoryNode> dirNode_S1 = subDirNodes[1];
        dirNode_S1->getSubDirs(subDirNodes);
        std::shared_ptr<SourceDirectoryNode> dirNode_S1_S2 = subDirNodes[2];

        // ...and mark these nodes dirty
        dirNode->setState(Node::State::Dirty);
        dirNode_S1->setState(Node::State::Dirty);
        dirNode_S1_S2->setState(Node::State::Dirty);


        context.statistics().reset();
        context.statistics().registerNodes = true;
        // re-execute directory node tree to sync with changed testTree
        completed = YAMTest::executeNode(dirNode.get());
        EXPECT_TRUE(completed);

        verify(&testTree, dirNode.get());

        // 13 nStarted and nSelfExecuted include the .ignore, .gitignore and
        // .yamignore of the added dir in testTree_S1_S2 
        EXPECT_EQ(13, context.statistics().nStarted);
        // 10 because SourceDirectoryNode::pendingStartSelf always returns true.
        // But only 4 dir nodes will see a modified directory. Only those 4
        // update their content.
        EXPECT_EQ(13, context.statistics().nSelfExecuted);
        // 8 nRehashedFiles include .gitignore and .yamignore of the added dir
        // in testTree_S1_S2 
        EXPECT_EQ(8, context.statistics().nRehashedFiles);
        EXPECT_EQ(4, context.statistics().nDirectoryUpdates);
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode.get()));
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1.get()));
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1_S2.get()));
        std::vector<std::shared_ptr<FileNode>> files;
        dirNode->getFiles(files);
        EXPECT_TRUE(context.statistics().rehashedFiles.contains(files[files.size() - 1].get()));

        dirNode_S1->getFiles(files);
        EXPECT_TRUE(context.statistics().rehashedFiles.contains(files[files.size() - 1].get()));

        dirNode_S1_S2->getSubDirs(subDirNodes);
        auto dirNode_S1_S2_S3 = subDirNodes[subDirNodes.size() - 1];
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1_S2_S3.get()));
        dirNode_S1_S2_S3->getFiles(files);
        for (auto f : files) EXPECT_TRUE(context.statistics().rehashedFiles.contains(f.get()));
    }
}
