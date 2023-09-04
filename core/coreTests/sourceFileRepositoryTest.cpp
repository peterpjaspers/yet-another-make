#include "executeNode.h"
#include "DirectoryTree.h"
#include "../SourceFileRepository.h"
#include "../SourceDirectoryNode.h"
#include "../SourceFileNode.h"
#include "../ExecutionContext.h"
#include "../RegexSet.h"
#include "../FileSystem.h"
#include "../Dispatcher.h"
#include "../DispatcherFrame.h"
#include "../../xxhash/xxhash.h"

#include "gtest/gtest.h"
#include <chrono>
#include <string>
#include <fstream>
#include <cstdio>

namespace
{
    using namespace YAMTest;
    using namespace YAM;

    auto isDirtyNode = Delegate<bool, Node*>::CreateLambda([](Node* n) { return n->state() == Node::State::Dirty; });

    void findDirtyNodes(
        SourceDirectoryNode* dirNode, 
        std::vector<Node*>& dirtyNodes, 
        std::unordered_set<Node*>& visitedNodes
    ) {
        auto const& content = dirNode->getContent();
        for (auto const& pair : content) {
            if (!visitedNodes.insert(pair.second.get()).second) return; // node was already visited
            if (pair.second->state() == Node::State::Dirty) {
                dirtyNodes.push_back(pair.second.get());
            }
            auto childDir = dynamic_cast<SourceDirectoryNode*>(pair.second.get());
            if (childDir != nullptr) {
                findDirtyNodes(childDir, dirtyNodes, visitedNodes);
            }
        }
    }
    std::vector<Node*> getDirtyNodes(SourceDirectoryNode* dirNode) {
        std::vector<Node*> dirtyNodes;
        std::unordered_set<Node*> visitedNodes;
        findDirtyNodes(dirNode, dirtyNodes, visitedNodes);
        return dirtyNodes;
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
        SourceDirectoryNode* dirNode = repo->directoryNode().get();

        repo->consumeChanges();
        std::vector<Node*> dirtyNodes = getDirtyNodes(dirNode);
        EXPECT_EQ(1, dirtyNodes.size());
        EXPECT_EQ(dirNode, dirtyNodes[0]);
        bool completed = YAMTest::executeNodes(dirtyNodes);
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode);

        std::vector<std::shared_ptr<SourceDirectoryNode>> subDirNodes;
        dirNode->getSubDirs(subDirNodes);
        std::shared_ptr<SourceDirectoryNode> dirNode_S1 = subDirNodes[1];
        dirNode_S1->getSubDirs(subDirNodes);
        std::shared_ptr<SourceDirectoryNode> dirNode_S1_S2 = subDirNodes[2];

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

        context.statistics().reset();
        context.statistics().registerNodes = true;
        completed = YAMTest::executeNodes(dirtyNodes);
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode);

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