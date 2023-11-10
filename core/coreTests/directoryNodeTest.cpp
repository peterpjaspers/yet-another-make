#include "gtest/gtest.h"
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../ExecutionContext.h"
#include "../../xxhash/xxhash.h"

#include <chrono>

namespace
{
    using namespace YAM;
    using namespace YAMTest;

    std::chrono::seconds timeout(10);
    TEST(DirectoryNode, construct_twoDeepDirectoryTree) {
        std::string tmpDir(std::tmpnam(nullptr));
        std::filesystem::path rootDir(std::string(tmpDir + "_dirNodeTest"));
        DirectoryTree testTree(rootDir, 2, RegexSet());

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        auto repo = std::make_shared<FileRepository>("repo", rootDir, &context);
        context.addRepository(repo);
        auto dirNode = repo->directoryNode();
        EXPECT_EQ(Node::State::Dirty, dirNode->state());

        bool completed = YAMTest::executeNode(dirNode.get());
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode.get());
    }

    TEST(DirectoryNode, update_threeDeepDirectoryTree) {
        std::string tmpDir(std::tmpnam(nullptr));
        std::filesystem::path rootDir(std::string(tmpDir + "_dirNodeTest"));
        DirectoryTree testTree(rootDir, 3, RegexSet());

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        auto repo = std::make_shared<FileRepository>("repo", rootDir, &context);
        context.addRepository(repo);
        auto dirNode = repo->directoryNode();
        bool completed = YAMTest::executeNode(dirNode.get());
        EXPECT_TRUE(completed);

        // Update file system
        DirectoryTree* testTree_S1 = testTree.getSubDirs()[1];
        DirectoryTree* testTree_S1_S2 = testTree_S1->getSubDirs()[2];
        testTree.addFile();// adds file4
        testTree_S1->addFile();// adds sub2\file4
        testTree_S1_S2->addDirectory(); // adds sub3\sub4 with 3 files and 3 subdirs
        testTree_S1_S2->addFile(); // adds sub4\file4

        // Find nodes affected by file system changes...
        std::vector<std::shared_ptr<DirectoryNode>> subDirNodes;
        dirNode->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S1 = subDirNodes[1];
        dirNode_S1->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S1_S2 = subDirNodes[2];

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

        // Start are dir nodes testTree, sub2, sub3, sub4 and the 3 .ignore nodes
        // of sub4. 
        EXPECT_EQ(7, context.statistics().nStarted);
        EXPECT_EQ(7, context.statistics().nSelfExecuted);

        // 2 sub4\.gitignore and sub4\.yamignore
        EXPECT_EQ(2, context.statistics().nRehashedFiles);
        EXPECT_EQ(2, context.statistics().rehashedFiles.size());
        dirNode_S1_S2->getSubDirs(subDirNodes);
        auto dirNode_S1_S2_S3 = subDirNodes[subDirNodes.size() - 1];
        auto gitignore = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(dirNode_S1_S2_S3->name() / ".gitignore"));
        auto yamignore = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(dirNode_S1_S2_S3->name() / ".yamignore"));
        EXPECT_TRUE(context.statistics().rehashedFiles.contains(gitignore.get()));
        EXPECT_TRUE(context.statistics().rehashedFiles.contains(yamignore.get()));

        EXPECT_EQ(4, context.statistics().nDirectoryUpdates);
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode.get()));
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1.get()));
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1_S2.get()));
    }
}
