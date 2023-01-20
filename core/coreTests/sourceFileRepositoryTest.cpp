#include "gtest/gtest.h"
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../SourceFileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../ExecutionContext.h"
#include "../RegexSet.h"
#include "../../xxhash/xxhash.h"

#include <chrono>
#include <string>
#include <fstream>
#include <cstdio>

namespace
{
    using namespace YAMTest;
    using namespace YAM;
    TEST(SourceFileRepository, update_threeDeepDirectoryTree) {
        std::string tmpDir(std::tmpnam(nullptr));
        std::filesystem::path rootDir(std::string(tmpDir + "_dirNodeTest"));
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

        std::vector<std::shared_ptr<Node>> dirtyNodes;
        context.getDirtyNodes(dirtyNodes);
        EXPECT_EQ(1, dirtyNodes.size()); 
        EXPECT_EQ(dirNode, dirtyNodes[0].get());
        repo->suspend();
        bool completed = YAMTest::executeNodes(dirtyNodes);
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode);
        repo->resume();
        // Although no dirty nodes are expected added an EXPECT_EQ to
        // to be notified of any future change in behavior.
        // See spuriousChangeEvents in directoryWatcherTest.cpp.
        context.getDirtyNodes(dirtyNodes);
        // Although there are 39 (3+9+27)subdirs we only get change events for
        // the 36 (9+27) subdirs below rootDir\SubDir1..3
        EXPECT_EQ(36, dirtyNodes.size());

        // Repeat execution to get rid of the dirty nodes caused by the
        // spurious change events. See spuriousChangeEvents in directoryWatcherTest.cpp.
        // Note that the DirectoryNodes, although Dirty, will not re-iterate
        // the directories because directory modification times have not changed.
        context.getDirtyNodes(dirtyNodes);
        repo->suspend();
        completed = YAMTest::executeNodes(dirtyNodes);
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode);
        repo->resume();
        context.getDirtyNodes(dirtyNodes);
        EXPECT_EQ(0, dirtyNodes.size());

        // Update file system
        DirectoryTree* testTree_S1 = testTree.getSubDirs()[1];
        DirectoryTree* testTree_S1_S2 = testTree_S1->getSubDirs()[2];
        testTree.addFile();// adds 4-th file
        testTree_S1->addFile();// adds 4-th file
        testTree_S1_S2->addDirectory(); // adds 1 dir with 3 files and 3 subdirs
        testTree_S1_S2->addFile(); // adds 4-th file

        // Find nodes affected by file system changes...
        std::vector<std::shared_ptr<DirectoryNode>> subDirNodes;
        dirNode->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S1 = subDirNodes[1];
        dirNode_S1->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S1_S2 = subDirNodes[2];

        context.statistics().reset();
        context.statistics().registerNodes = true; 
        context.getDirtyNodes(dirtyNodes);
        repo->suspend();
        completed = YAMTest::executeNodes(dirtyNodes);
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode);
        repo->resume();
        // Now that directories have been re-iterated we again have spurious
        // change events causing nodes to be set Dirty: rootDir, rootDir\SubDir2 
        // and rootDir\subDir2\subDir3
        EXPECT_EQ(dirtyNodes.size(), 3);
        
        EXPECT_EQ(10, context.statistics().nStarted);
        // 10 because DirectoryNode::pendingStartSelf always returns true.
        // But only 4 dir nodes will see a modified directory. Only those 4
        // update their content.
        EXPECT_EQ(10, context.statistics().nSelfExecuted);
        EXPECT_EQ(6, context.statistics().nFileUpdates);
        EXPECT_EQ(4, context.statistics().nDirectoryUpdates);
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode));
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1.get()));
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1_S2.get()));
        std::vector<std::shared_ptr<FileNode>> files;
        dirNode->getFiles(files);
        EXPECT_TRUE(context.statistics().updatedFiles.contains(files[files.size() - 1].get()));

        dirNode_S1->getFiles(files);
        EXPECT_TRUE(context.statistics().updatedFiles.contains(files[files.size() - 1].get()));

        dirNode_S1_S2->getSubDirs(subDirNodes);
        auto dirNode_S1_S2_S3 = subDirNodes[subDirNodes.size() - 1];
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1_S2_S3.get()));
        dirNode_S1_S2_S3->getFiles(files);
        for (auto f : files) EXPECT_TRUE(context.statistics().updatedFiles.contains(f.get()));
    }
}