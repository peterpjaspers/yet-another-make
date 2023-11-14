#include "executeNode.h"
#include "DirectoryTree.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
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
    using namespace YAM;

    class RepoProps {
    public:
        RepoProps()
            : name("testRepo")
            , dir(R"(C:\aap\noot\mies)")
        {
            std::filesystem::remove_all(dir);
            std::filesystem::create_directories(dir);
        }
        ~RepoProps() {
            std::filesystem::remove_all(dir);
        }
        std::string name;
        std::filesystem::path dir;
    };

    TEST(FileRepository, construct) {
        RepoProps repoProps;
        ExecutionContext context;
        FileRepository repo(repoProps.name, repoProps.dir, &context);
        EXPECT_EQ(repoProps.name, repo.name());
        EXPECT_EQ(repoProps.dir, repo.directory());
    }
    TEST(FileRepository, lexicallyContains) {
        RepoProps repoProps;
        ExecutionContext context;
        FileRepository repo(repoProps.name, repoProps.dir, &context);
        EXPECT_TRUE(repo.lexicallyContains(R"(C:\aap\noot\mies\file.cpp)"));
        EXPECT_TRUE(repo.lexicallyContains(R"(C:\aap\noot\mies)"));
        EXPECT_TRUE(repo.lexicallyContains(R"(C:\aap\noot\mies\)"));
        EXPECT_TRUE(repo.lexicallyContains(R"(testRepo)"));
        EXPECT_TRUE(repo.lexicallyContains(R"(testRepo\)"));
        EXPECT_TRUE(repo.lexicallyContains(R"(testRepo\file.cpp)"));

        EXPECT_FALSE(repo.lexicallyContains(R"(unknown\file.cpp)"));
        EXPECT_FALSE(repo.lexicallyContains(R"(C:\aap\noot\file.cpp)"));
        EXPECT_FALSE(repo.lexicallyContains(R"(\aap\noot\mies\file.cpp)"));
        EXPECT_FALSE(repo.lexicallyContains(R"(aap\noot\mies\file.cpp)"));
    }
    TEST(FileRepository, relativePath) {
        RepoProps repoProps;
        ExecutionContext context;
        FileRepository repo(repoProps.name, repoProps.dir, &context);
        EXPECT_EQ(std::filesystem::path(R"(file.cpp)"), repo.relativePathOf(R"(C:\aap\noot\mies\file.cpp)"));
        EXPECT_ANY_THROW(repo.relativePathOf(R"(testRepo)"));
        EXPECT_EQ(std::filesystem::path(), repo.relativePathOf(R"(C:\aap\noot\mies)"));
        EXPECT_EQ(std::filesystem::path(), repo.relativePathOf(R"(C:\aap\noot\mies\)"));
        EXPECT_EQ(std::filesystem::path(), repo.relativePathOf(R"(C:\aap\noot\file.cpp)"));
        EXPECT_ANY_THROW(repo.relativePathOf(R"(\aap\noot\file.cpp)"));
        EXPECT_ANY_THROW(repo.relativePathOf(R"(aap\noot\mies\file.cpp)"));
    }
    TEST(FileRepository, symbolicPath) {
        RepoProps repoProps;
        ExecutionContext context;
        FileRepository repo(repoProps.name, repoProps.dir, &context);
        EXPECT_EQ(std::filesystem::path(R"(testRepo\file.cpp)"), repo.symbolicPathOf(R"(C:\aap\noot\mies\file.cpp)"));
        EXPECT_EQ(std::filesystem::path(R"(testRepo)"), repo.symbolicPathOf(R"(C:\aap\noot\mies)"));
        EXPECT_EQ(std::filesystem::path(R"(testRepo\)"), repo.symbolicPathOf(R"(C:\aap\noot\mies\)"));
        EXPECT_EQ(std::filesystem::path(), repo.symbolicPathOf(R"(C:\aap\noot\file.cpp)"));
        EXPECT_ANY_THROW(repo.symbolicPathOf(R"(\aap\noot\file.cpp)"));
        EXPECT_ANY_THROW(repo.symbolicPathOf(R"(aap\noot\mies\file.cpp)"));
    }
}
namespace
{
    using namespace YAMTest;
    using namespace YAM;

    auto isDirtyNode = Delegate<bool, Node*>::CreateLambda([](Node* n) { return n->state() == Node::State::Dirty; });

    void findDirtyNodes(
        Node* node, 
        std::vector<Node*>& dirtyNodes, 
        std::unordered_set<Node*>& visitedNodes
    ) {
        if (!visitedNodes.insert(node).second) return; // node was already visited
        if (node->state() == Node::State::Dirty) {
            dirtyNodes.push_back(node);
        }
        auto dirNode = dynamic_cast<DirectoryNode*>(node);
        if (dirNode != nullptr) {
            auto const& content = dirNode->getContent();
            for (auto const& pair : content) {
                findDirtyNodes(pair.second.get(), dirtyNodes, visitedNodes);
            }
        }
    }

    std::vector<Node*> getDirtyNodes(DirectoryNode* dirNode) {
        std::vector<Node*> dirtyNodes;
        std::unordered_set<Node*> visitedNodes;
        findDirtyNodes(dirNode, dirtyNodes, visitedNodes);
        return dirtyNodes;
    }

    TEST(FileRepository, update_threeDeepDirectoryTree) {
        std::filesystem::path tmpDir = FileSystem::createUniqueDirectory();
        std::filesystem::path rootDir(tmpDir.string() + "_dirNodeTest");
        RegexSet excludes;
        DirectoryTree testTree(rootDir, 3, excludes);

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        auto repo = std::make_shared<FileRepository>(
            std::string("testRepo"), 
            rootDir,
            &context);
        context.addRepository(repo);
        DirectoryNode* dirNode = repo->directoryNode().get();

        repo->consumeChanges();
        std::vector<Node*> dirtyNodes = getDirtyNodes(dirNode);
        EXPECT_EQ(1, dirtyNodes.size());
        EXPECT_EQ(dirNode, dirtyNodes[0]);
        bool completed = YAMTest::executeNodes(dirtyNodes);
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode);
        dirtyNodes = getDirtyNodes(dirNode);
        ASSERT_EQ(120, dirtyNodes.size()); // All file nodes are dirty

        std::vector<std::shared_ptr<DirectoryNode>> subDirNodes;
        dirNode->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S1 = subDirNodes[1];
        dirNode_S1->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S1_S2 = subDirNodes[2];

        while (!dirtyNodes.empty()) {
            completed = YAMTest::executeNodes(dirtyNodes);
            EXPECT_TRUE(completed);
            repo->consumeChanges();
            dirtyNodes = getDirtyNodes(dirNode);
        }
        // Update file system. Note: 6 files are added and 4 dirs
        DirectoryTree* testTree_S1 = testTree.getSubDirs()[1];
        DirectoryTree* testTree_S1_S2 = testTree_S1->getSubDirs()[2];
        testTree.addFile();// adds 4-th file
        testTree_S1->addFile();// adds 4-th file
        testTree_S1_S2->addDirectory(); // adds 1 dir with 3 files
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
        ASSERT_EQ(3, dirtyNodes.size()); // testRepo, testRepo\subDir2, testRepo\subDir2\subDir3

        context.statistics().reset();
        context.statistics().registerNodes = true;
        // This execution detects the 1 new dirs and 6 new files and
        // creates 1 dir node and 6 file nodes.
        // The dir nodes are executed, the file nodes are not.
        completed = YAMTest::executeNodes(dirtyNodes);
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode);
        dirtyNodes = getDirtyNodes(dirNode);
        ASSERT_EQ(6, dirtyNodes.size()); // the 6 added file nodes

        std::vector<const Node*> fileNodes;
        std::vector<const Node*> dirNodes;
        std::vector<const Node*> otherNodes;
        for (auto n : context.statistics().started) {
            if (dynamic_cast<const FileNode*>(n)) fileNodes.push_back(n);
            else if (dynamic_cast<const DirectoryNode*>(n)) dirNodes.push_back(n);
            else otherNodes.push_back(n);
        }
        // subDir4\.yamignore + subDir4\.gitignore + subDir4\dotIgnore
        // + testRepo + subDir2 + subDir2\subDir3 + subDir2\subDir3\subDir4
        // 
        EXPECT_EQ(7, context.statistics().nStarted);
        EXPECT_EQ(7, context.statistics().nSelfExecuted);
        // subDir4\.yamignore + subDir4\.gitignore
        EXPECT_EQ(2, context.statistics().nRehashedFiles);
        // testRepo + subDir2 + subDir2\subDir3 + subDir2\subDir3\subDir4
        EXPECT_EQ(4, context.statistics().nDirectoryUpdates);
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode));
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1.get()));
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1_S2.get()));

        // The new file4 files are newly detected and therefore not executed (hashed)
        std::vector<std::shared_ptr<FileNode>> files;
        dirNode->getFiles(files);
        EXPECT_FALSE(context.statistics().rehashedFiles.contains(files[files.size() - 1].get()));

        dirNode_S1->getFiles(files);
        EXPECT_FALSE(context.statistics().rehashedFiles.contains(files[files.size() - 1].get()));

        dirNode_S1_S2->getSubDirs(subDirNodes);
        auto dirNode_S1_S2_S3 = subDirNodes[subDirNodes.size() - 1];
        EXPECT_TRUE(context.statistics().updatedDirectories.contains(dirNode_S1_S2_S3.get()));
        dirNode_S1_S2_S3->getFiles(files);
        for (auto f : files) EXPECT_FALSE(context.statistics().rehashedFiles.contains(f.get()));
    }
}