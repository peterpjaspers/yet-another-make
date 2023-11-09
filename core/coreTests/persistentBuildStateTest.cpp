
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../SourceFileRepository.h"
#include "../SourceDirectoryNode.h"
#include "../SourceFileNode.h"
#include "../DotIgnoreNode.h"
#include "../ExecutionContext.h"
#include "../DotYamDirectory.h"
#include "../PersistentBuildState.h"
#include "../FileSystem.h"
#include "../RegexSet.h"
#include "../FileAspect.h"

#include "gtest/gtest.h"
#include <chrono>
#include <memory>

namespace
{
    using namespace YAM;
    using namespace YAMTest;

    // repoDir contains .yam dir, buildstate dir, subdirs 1,2,3 and files 1,2,3
    // Each subdir contains 39 files and 12 directories.
    // Including repoDir and .yam dir: 42 dirs, 120 files.
    // Nodes: per directory 4 nodes (dir node, dotignore .yamignore and
    // .gitignore). Per file 1 node. Total nodes: 42*4 + 120 = 288
    const std::size_t nNodes = 288; // in context->nodes()

    // Wait for file change event to be received for given paths.
    // When event is received then consume the changes.
    // Return whether event was consumed.
    bool consumeFileChangeEvents(
        SourceFileRepository* sourceFileRepo,
        Dispatcher& mainThreadQueue,
        std::initializer_list<std::filesystem::path> paths)
    {
        std::atomic<bool> received = false;
        Dispatcher dispatcher;
        auto pollChange = Delegate<void>::CreateLambda(
            [&]() {
            for (auto path : paths) {
                received = sourceFileRepo->hasChanged(path);
                if (!received) break;
            }
        if (received) {
            sourceFileRepo->consumeChanges();
        }
        dispatcher.stop();
        });
        const auto retryInterval = std::chrono::milliseconds(100);
        const unsigned int maxRetries = 10;
        unsigned int nRetries = 0;
        do {
            nRetries++;
            dispatcher.start();
            mainThreadQueue.push(pollChange);
            dispatcher.run();
            if (!received) std::this_thread::sleep_for(retryInterval);
        } while (nRetries < maxRetries && !received);
        return received;
    }

    // Create directory tree, mirror tree in source directory node and
    // store resulting directory node tree to repoDir/ "nodes"
    class SetupHelper
    {
    public:

        std::filesystem::path repoDir;
        std::filesystem::path yamDir;
        DirectoryTree testTree;
        ExecutionContext context;
        PersistentBuildState persistentState;
        std::shared_ptr<SourceFileRepository> sourceFileRepo;
        std::shared_ptr<SourceDirectoryNode> repoDirNode;

        SetupHelper(std::filesystem::path const& repoDirectory)
            : repoDir(repoDirectory)
            , yamDir(DotYamDirectory::create(repoDir))
            , testTree(repoDir, 3, RegexSet({ ".yam" }))
            , persistentState(repoDir / "buildState", &context)
            , sourceFileRepo(std::make_shared<SourceFileRepository>(
                "repo",
                repoDir,
                &context))
            , repoDirNode(sourceFileRepo->directoryNode())
        {
            context.threadPool().size(1);
            context.addRepository(sourceFileRepo);
            bool completed = YAMTest::executeNode(repoDirNode.get());
            EXPECT_TRUE(completed);
            EXPECT_EQ(nNodes, context.nodes().size());
            persistentState.store();
        }

        bool consumeFileChangeEvent(std::initializer_list<std::filesystem::path> paths) {
            return consumeFileChangeEvents(sourceFileRepo.get(), context.mainThreadQueue(), paths);
        }

        void updateFile(std::filesystem::path const& fileToUpdate) {
            std::ofstream file(fileToUpdate.string());
            file << "Add some content to the file";
            file.close();
        }

        std::filesystem::path addNode() {
            std::filesystem::path file4(repoDir / "File4");
            testTree.addFile(); // File4
            EXPECT_TRUE(consumeFileChangeEvent({ file4 }));
            std::vector<std::shared_ptr<Node>> dirtyNodes;
            context.getDirtyNodes(dirtyNodes);
            EXPECT_TRUE(YAMTest::executeNodes(dirtyNodes));
            EXPECT_NE(nullptr, context.nodes().find(file4));
            return file4;
        }

        std::tuple<std::filesystem::path, XXH64_hash_t> modifyNode() {
            std::filesystem::path file3(repoDir / "File3");
            auto node = dynamic_pointer_cast<FileNode>(context.nodes().find(file3));
            XXH64_hash_t oldHash = node->hashOf(FileAspect::entireFileAspect().name());
            updateFile(file3);
            consumeFileChangeEvent({ file3 });
            YAMTest::executeNode(node.get());
            XXH64_hash_t newHash = node->hashOf(FileAspect::entireFileAspect().name());
            EXPECT_NE(oldHash, newHash);
            return { file3, oldHash };
        }

        void executeAll() {
            auto setDirty = Delegate<void, std::shared_ptr<Node> const&>::CreateLambda(
                [](std::shared_ptr<Node> const& n) { n->setState(Node::State::Dirty); });
            context.nodes().foreach(setDirty);
            std::vector<std::shared_ptr<Node>> dirtyNodes;
            context.getDirtyNodes(dirtyNodes);
            bool completed = YAMTest::executeNodes(dirtyNodes);
            EXPECT_TRUE(completed);
        }
    };

    class StorageHelper
    {
    public:
        SetupHelper& setup;
        std::shared_ptr<SourceDirectoryNode> repoDirNode;

        StorageHelper(SetupHelper& setupHelper)
            : setup(setupHelper)
        {
        }

        std::shared_ptr<SourceDirectoryNode> retrieve() {
            setup.persistentState.retrieve();
            return dynamic_pointer_cast<SourceDirectoryNode>(setup.context.nodes().find(setup.repoDir));
        }

        std::size_t store() {
            return setup.persistentState.store();
        }

        bool consumeFileChangeEvent(std::initializer_list<std::filesystem::path> paths) {
            auto fileRepo = setup.context.findRepository("repo");
            return consumeFileChangeEvents(fileRepo.get(),setup.context.mainThreadQueue(), paths);
        }

        XXH64_hash_t addFileAndUpdateFileAndExecuteNode(std::shared_ptr<FileNode> fileNode) {
            auto hash = fileNode->hashOf(FileAspect::entireFileAspect().name());
            setup.testTree.addFile(); // add File4
            setup.updateFile(fileNode->name());
            bool consumed = consumeFileChangeEvent({ fileNode->name(), setup.repoDir / "File4" });
            if (!consumed || Node::State::Dirty != fileNode->state()) throw std::exception("wrong state");
            // ... execute the file to recompute the file hash...
            std::vector<std::shared_ptr<Node>> dirtyNodes;
            setup.context.getDirtyNodes(dirtyNodes);
            bool completed = YAMTest::executeNodes(dirtyNodes);
            EXPECT_TRUE(completed);
            // ... and verify that hash has changed
            auto updatedHash = fileNode->hashOf(FileAspect::entireFileAspect().name());
            EXPECT_NE(hash, updatedHash);
            return updatedHash;
        }
    };

    TEST(PersistentBuildState, storeAndRetrieve) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        // Retrieve the nodes stored by setup.
        StorageHelper storage(setup);
        auto repoDirNode = storage.retrieve();
        ASSERT_NE(nullptr, repoDirNode);
        EXPECT_EQ(nNodes, setup.context.nodes().size());

        // Verify that the rolled-back build state can be executed
        bool completed = YAMTest::executeNode(repoDirNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(nNodes, setup.context.nodes().size());
        storage.store(); // executeNode put all nodes to modified state

        // Verify that re-executing a file node sets it modified.
        auto fileNode = dynamic_pointer_cast<FileNode>(setup.context.nodes().find(setup.repoDir / "File3"));
        ASSERT_NE(nullptr, fileNode);
        auto updatedHash = storage.addFileAndUpdateFileAndExecuteNode(fileNode);
        EXPECT_TRUE(fileNode->modified());
        EXPECT_EQ(nNodes+1, setup.context.nodes().size()); // new FileNode for File4
        auto newFileNode = dynamic_pointer_cast<FileNode>(setup.context.nodes().find(setup.repoDir / "File4"));
        EXPECT_NE(nullptr, newFileNode);
        EXPECT_TRUE(newFileNode->modified());

        // Verify that the modified file node is updated in storage.
        std::size_t nStored = storage.store(); // store the modified file node.
        EXPECT_EQ(3, nStored); // repo dir, file3, file4
        storage.retrieve(); // replace all nodes in storage.context by ones freshly retrieved from storage
        fileNode = dynamic_pointer_cast<FileNode>(setup.context.nodes().find(setup.repoDir / "File3"));
        ASSERT_NE(nullptr, fileNode);

        EXPECT_FALSE(fileNode->modified());
        newFileNode = dynamic_pointer_cast<FileNode>(setup.context.nodes().find(setup.repoDir / "File4"));
        EXPECT_NE(nullptr, newFileNode);
        EXPECT_FALSE(newFileNode->modified());

        auto actualHash = fileNode->hashOf(FileAspect::entireFileAspect().name());
        EXPECT_EQ(updatedHash, actualHash);
    }

    TEST(PersistentBuildState, rollbackRemovedRepo) {
        SetupHelper setup(FileSystem::createUniqueDirectory());
        EXPECT_EQ(nNodes, setup.context.nodes().size());

        std::string repoName = setup.sourceFileRepo->name();
        EXPECT_TRUE(setup.context.removeRepository(repoName));
        EXPECT_EQ(0, setup.context.nodes().size());
        setup.sourceFileRepo = nullptr;
        setup.repoDirNode = nullptr;

        setup.persistentState.rollback();

        EXPECT_EQ(nNodes, setup.context.nodes().size());
        EXPECT_NE(nullptr, setup.context.findRepository(repoName));
        // Verify that the rolled-back build state can be executed
        setup.executeAll();
    }

    TEST(PersistentBuildState, rollbackRemovedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());
        EXPECT_EQ(nNodes, setup.context.nodes().size());

        std::map<std::filesystem::path, std::shared_ptr<Node>> dirContentBeforeClear =  
            setup.repoDirNode->getContent();
        EXPECT_EQ(8, dirContentBeforeClear.size());
        setup.sourceFileRepo->clear(); // will remove all nodes
        EXPECT_EQ(0, setup.context.nodes().size());
        EXPECT_EQ(0, setup.repoDirNode->getContent().size());

        setup.persistentState.rollback();

        EXPECT_EQ(nNodes, setup.context.nodes().size());
        EXPECT_EQ(dirContentBeforeClear.size(), setup.repoDirNode->getContent().size());
        // Verify that the rolled-back build state can be executed
        setup.executeAll();
    }

    TEST(PersistentBuildState, rollbackAddedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());
        EXPECT_EQ(nNodes, setup.context.nodes().size());

        std::filesystem::path addedFile = setup.addNode();
        // +2 because besides a node for the added file also a
        // node for buildstate\buildstate.bt file has been created.
        EXPECT_EQ(nNodes + 2, setup.context.nodes().size());

        setup.persistentState.rollback();

        EXPECT_EQ(nNodes, setup.context.nodes().size());
        EXPECT_EQ(nullptr, setup.context.nodes().find(addedFile));
        // Verify that the rolled-back build state can be executed
        setup.executeAll();
    }

    TEST(PersistentBuildState, rollbackModifiedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());
        EXPECT_EQ(nNodes, setup.context.nodes().size());

        auto tuple = setup.modifyNode();
        std::filesystem::path modifiedFile = std::get<0>(tuple);
        XXH64_hash_t hashBeforeModify = std::get<1>(tuple);

        setup.persistentState.rollback();

        EXPECT_EQ(nNodes, setup.context.nodes().size());
        auto node = dynamic_pointer_cast<FileNode>(setup.context.nodes().find(modifiedFile));
        XXH64_hash_t hash = node->hashOf(FileAspect::entireFileAspect().name());
        EXPECT_EQ(hashBeforeModify, hash);
        // Verify that the rolled-back build state can be executed
        setup.executeAll();
    }
}
