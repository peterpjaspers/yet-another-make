#include "gtest/gtest.h"
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../SourceFileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../ExecutionContext.h"
#include "../RegexSet.h"
#include "../GraphWalker.h"
#include "../FileSystem.h"
#include "../Dispatcher.h"
#include "../DispatcherFrame.h"
#include "../../xxhash/xxhash.h"

#include <chrono>
#include <string>
#include <fstream>
#include <cstdio>

namespace
{
    using namespace YAMTest;
    using namespace YAM;

    auto isDirtyNode = Delegate<bool, Node*>::CreateLambda([](Node* n) { return n->state() == Node::State::Dirty; });

    std::vector<Node*> getDirtyNodes(DirectoryNode* dirNode) {
        GraphWalker walker(dirNode, GraphWalker::Postrequisites, isDirtyNode);
        return walker.included();
    }

    TEST(SourceFileRepository, update_threeDeepDirectoryTree) {
        std::filesystem::path tmpDir = FileSystem::createUniqueDirectory();
        std::filesystem::path rootDir(tmpDir.string() + "_dirNodeTest");
        RegexSet excludes;
        DirectoryTree testTree(rootDir, 3, excludes);

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        auto repo = std::make_shared<SourceFileRepository>(
            std::string("testRepo"), 
            rootDir,
            excludes,
            &context);
        context.addRepository(repo);
        DirectoryNode* dirNode = repo->directory().get();

        repo->consumeChanges();
        std::vector<Node*> dirtyNodes = getDirtyNodes(dirNode);
        EXPECT_EQ(1, dirtyNodes.size());
        EXPECT_EQ(dirNode, dirtyNodes[0]);
        repo->buildInProgress(true);
        bool completed = YAMTest::executeNodes(dirtyNodes);
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode);
        repo->buildInProgress(false);

        std::vector<std::shared_ptr<DirectoryNode>> subDirNodes;
        dirNode->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S1 = subDirNodes[1];
        dirNode_S1->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S1_S2 = subDirNodes[2];

        // Update file system
        DirectoryTree* testTree_S1 = testTree.getSubDirs()[1];
        DirectoryTree* testTree_S1_S2 = testTree_S1->getSubDirs()[2];
        testTree.addFile();// adds 4-th file
        testTree_S1->addFile();// adds 4-th file
        testTree_S1_S2->addDirectory(); // adds 1 dir with 3 files and 3 subdirs
        testTree_S1_S2->addFile(); // adds 4-th file

        // Wait until file change events have resulted in nodes to become
        // dirty. Take care: node states are updated in main thread. Hence
        // getting node states in test thread is not reliable. Therefore
        // retrieve node states in main thread.
        Dispatcher dispatcher;
        auto fillDirtyNodes = Delegate<void>::CreateLambda(
            [&]() {
                repo->consumeChanges();
                dirtyNodes = getDirtyNodes(dirNode);
                dispatcher.stop();
            });
        const auto retryInterval = std::chrono::milliseconds(1000);
        const unsigned int maxRetries = 5;
        unsigned int nRetries = 0;
        bool done;
        do {
            nRetries++;
            dispatcher.start();
            context.mainThreadQueue().push(fillDirtyNodes);
            // dispatcher.run() will block until main thread executed fillDirtyNodes.
            dispatcher.run();
            done = dirtyNodes.size() == 3;
            if (!done) std::this_thread::sleep_for(retryInterval);
        } while (nRetries < maxRetries && !done);
        ASSERT_EQ(3, dirtyNodes.size());

        repo->buildInProgress(true);
        context.statistics().reset();
        context.statistics().registerNodes = true;
        completed = YAMTest::executeNodes(dirtyNodes);
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode);
        repo->buildInProgress(false);
        
        EXPECT_EQ(10, context.statistics().nStarted);
        // 10 because DirectoryNode::pendingStartSelf always returns true.
        // But only 4 dir nodes will see a modified directory. Only those 4
        // update their content.
        EXPECT_EQ(10, context.statistics().nSelfExecuted);
        EXPECT_EQ(6, context.statistics().nRehashedFiles);
        EXPECT_EQ(4, context.statistics().nDirectoryUpdates);
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode));
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