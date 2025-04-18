#define _CRT_SECURE_NO_WARNINGS

#include "gtest/gtest.h"
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../FileRepositoryNode.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../ExecutionContext.h"
#include "../BuildFileParserNode.h"
#include "../BuildFileCompilerNode.h"
#include "../RepositoriesNode.h"
#include "../xxhash.h"

#include <chrono>

#include "../../accessMonitor/Monitor.h"

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
        auto repo = std::make_shared<FileRepositoryNode>(&context, "repo", rootDir, FileRepositoryNode::RepoType::Build);
        auto repos = std::make_shared<RepositoriesNode>(&context, repo);
        context.repositoriesNode(repos);
        auto dirNode = repo->directoryNode();
        EXPECT_EQ(Node::State::Dirty, dirNode->state());

        AccessMonitor::enableMonitoring();
        bool completed = YAMTest::executeNode(dirNode.get());
        EXPECT_TRUE(completed);
        verify(&testTree, dirNode.get());
        AccessMonitor::disableMonitoring();
    }

    TEST(DirectoryNode, update_threeDeepDirectoryTree) {
        std::string tmpDir(std::tmpnam(nullptr));
        std::filesystem::path rootDir(std::string(tmpDir + "_dirNodeTest"));
        DirectoryTree testTree(rootDir, 3, RegexSet());

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        auto repo = std::make_shared<FileRepositoryNode>(&context, "repo", rootDir, FileRepositoryNode::RepoType::Build);
        auto repos = std::make_shared<RepositoriesNode>(&context, repo);
        context.repositoriesNode(repos);
        auto dirNode = repo->directoryNode();

        AccessMonitor::enableMonitoring();
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

        AccessMonitor::disableMonitoring();
    }

    TEST(DirectoryNode, findChild) {
        std::string tmpDir(std::tmpnam(nullptr));
        std::filesystem::path rootDir(std::string(tmpDir + "_dirNodeTest"));
        DirectoryTree testTree(rootDir, 3, RegexSet());

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        auto repo = std::make_shared<FileRepositoryNode>(&context, "repo", rootDir, FileRepositoryNode::RepoType::Build);
        repo->repoType(FileRepositoryNode::RepoType::Build);
        auto repos = std::make_shared<RepositoriesNode>(&context, repo);
        context.repositoriesNode(repos);
        auto dirNode = repo->directoryNode();

        AccessMonitor::enableMonitoring();
        bool completed = YAMTest::executeNode(dirNode.get());
        EXPECT_TRUE(completed);

        std::vector<std::shared_ptr<DirectoryNode>> subDirNodes;
        dirNode->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S2 = subDirNodes[1];
        dirNode_S2->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S2_S2 = subDirNodes[1];
        std::shared_ptr<DirectoryNode> dirNode_S2_S3 = subDirNodes[2];

        {
            auto file = std::filesystem::path("File1");
            auto child = dirNode->findChild(file);
            ASSERT_NE(nullptr, child);
            EXPECT_EQ(repo->symbolicPathOf(repo->directory() / file), child->name());
        }
        {
            auto file = std::filesystem::path(".\\File1");
            auto child = dirNode->findChild(file);
            ASSERT_NE(nullptr, child);
            EXPECT_EQ(repo->symbolicPathOf(repo->directory() / "File1"), child->name());
        }
        {
            auto file = std::filesystem::path("..\\..\\File1");
            auto child = dirNode_S2_S3->findChild(file);
            EXPECT_EQ(dirNode->findChild(std::filesystem::path("File1")), child);
        }
        {
            auto file = std::filesystem::path("..\\SubDir2\\File1");
            auto child = dirNode_S2_S3->findChild(file);
            ASSERT_NE(nullptr, child);
            EXPECT_EQ(R"(@@repo\SubDir2\SubDir2\File1)", child->name().string());
        }
        {
            auto file = std::filesystem::path("..\\SubDir2\\..\\File1");
            auto child = dirNode_S2->findChild(file);
            EXPECT_EQ(dirNode->findChild(std::filesystem::path("File1")), child);
        }
        {
            auto file = std::filesystem::path("..\\SubDir2\\.\\.\\..\\File1");
            auto child = dirNode_S2->findChild(file);
            EXPECT_EQ(dirNode->findChild(std::filesystem::path("File1")), child);
        }
        {
            auto file = std::filesystem::path("File5");
            auto child = dirNode->findChild(file);
            EXPECT_EQ(nullptr, child);
        }
        {
            auto file = std::filesystem::path("..\\..\\..\\File1");
            auto child = dirNode_S2_S3->findChild(file);
            EXPECT_EQ(nullptr, child);
        }

        AccessMonitor::disableMonitoring();
    }

    TEST(DirectoryNode, buildFileParserNode) {
        std::string tmpDir(std::tmpnam(nullptr));
        std::filesystem::path rootDir(std::string(tmpDir + "_dirNodeTest"));
        std::filesystem::path buildFilePath(rootDir / R"(buildfile_yam.txt)");
        DirectoryTree testTree(rootDir, 2, RegexSet());
        std::ofstream buildFileStream(buildFilePath.string().c_str());
        EXPECT_TRUE(buildFileStream.is_open());
        buildFileStream.close();

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        auto repo = std::make_shared<FileRepositoryNode>(&context, "repo", rootDir, FileRepositoryNode::RepoType::Build);
        auto repos = std::make_shared<RepositoriesNode>(&context, repo);
        context.repositoriesNode(repos);
        auto dirNode = repo->directoryNode();
        EXPECT_EQ(Node::State::Dirty, dirNode->state());

        AccessMonitor::enableMonitoring();
        bool completed = YAMTest::executeNode(dirNode.get());
        EXPECT_TRUE(completed);

        auto symBuildFilePath = repo->symbolicPathOf(buildFilePath);
        auto buildFile = dirNode->findChild(buildFilePath.filename());
        EXPECT_NE(nullptr, buildFile);
        EXPECT_EQ(buildFile, context.nodes().find(symBuildFilePath));
        std::filesystem::path buildFileParserNodeName = dirNode->name() / "__bfParser";
        auto node = context.nodes().find(buildFileParserNodeName);
        ASSERT_NE(nullptr, node);
        auto buildFileParserNode = dynamic_pointer_cast<BuildFileParserNode>(node);
        ASSERT_NE(nullptr, buildFileParserNode);
        EXPECT_EQ(dirNode->buildFileParserNode(), buildFileParserNode);
        EXPECT_EQ(buildFilePath, buildFileParserNode->buildFile()->absolutePath());


        std::filesystem::path buildFileCompilerNodeName = dirNode->name() / "__bfCompiler";
        node = context.nodes().find(buildFileCompilerNodeName);
        ASSERT_NE(nullptr, node);
        auto buildFileCompilerNode = dynamic_pointer_cast<BuildFileCompilerNode>(node);
        ASSERT_NE(nullptr, buildFileCompilerNode);
        EXPECT_EQ(dirNode->buildFileCompilerNode(), buildFileCompilerNode);
        EXPECT_EQ(buildFileParserNode, buildFileCompilerNode->buildFileParser());

        AccessMonitor::disableMonitoring();
    }
}
