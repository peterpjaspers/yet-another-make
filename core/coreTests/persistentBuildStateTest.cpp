
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../FileRepositoryNode.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../CommandNode.h"
#include "../DotIgnoreNode.h"
#include "../RepositoriesNode.h"
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

    // repoDir contains subdirs 1,2,3 and files 1,2,3
    // Note: .yamDir is excluded
    // Each subdir contains 39 files and 12 directories.
    // Including repoDir, dir: 40 dirs, 120 files.
    // Nodes: per directory 4 nodes (dir node, dotignore .yamignore and
    // .gitignore). Per file 1 node.
    // cmdNode, cmdNode1: 2 command nodes.
    // RepositoriesNode + home repo node + repositories.txt file node: 3
    // FileRepostoryNode has: FileExecConfigNode + SourceFileNode: 2
    const std::size_t nNodes = 40 * 4 + 120 + 2 + 3 + 2; // in context->nodes()

    // Wait for file change event to be received for given paths.
    // When event is received then consume the changes.
    // Return whether event was consumed.
    bool consumeFileChangeEvents(
        FileRepositoryNode* sourceFileRepo,
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

    std::filesystem::path getBuildStateFile(std::filesystem::path const& repoDir) {
        auto file = repoDir / ".yam\\buildState\\buildstate.bt";
        std::filesystem::create_directories(file.parent_path());
        return file;
    }
    class Repository
    {
        std::filesystem::path repoDir;
        std::filesystem::path yamDir;
        DirectoryTree testTree;
        ExecutionContext context;
        PersistentBuildState persistentState;

        Repository(std::filesystem::path const& repoDirectory)
            : repoDir(repoDirectory)
            , yamDir(DotYamDirectory::create(repoDir))
            , testTree(repoDir, 0, RegexSet({ ".yam" }))
            , persistentState(getBuildStateFile(repoDir), &context)
        {
            //context.threadPool().size(1);
            auto homeRepo = std::make_shared<FileRepositoryNode>(
                &context,
                "repo",
                repoDir);
            auto repos = std::make_shared<RepositoriesNode>(&context, homeRepo);
            context.repositoriesNode(repos);
        }
    };

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

        SetupHelper(std::filesystem::path const& repoDirectory)
            : repoDir(repoDirectory)
            , yamDir(DotYamDirectory::create(repoDir))
            , testTree(repoDir, 3, RegexSet({ ".yam" }))
            , persistentState(getBuildStateFile(repoDir), &context)
        {
            //context.threadPool().size(1);
            auto homeRepo = std::make_shared<FileRepositoryNode>(
                &context,
                "repo",
                repoDir);
            auto repos = std::make_shared<RepositoriesNode>(&context, homeRepo);
            context.repositoriesNode(repos);
            sourceFileRepo()->startWatching();

            bool completed = YAMTest::executeNode(sourceFileRepo()->directoryNode().get());
            EXPECT_TRUE(completed);

            std::vector<std::shared_ptr<DirectoryNode>> subDirs; 
            sourceFileRepo()->directoryNode()->getSubDirs(subDirs); 
            auto cmdNode = std::make_shared<CommandNode>(&context, std::filesystem::path("@@repo") / "__cmd");
            cmdNode->script(R"(C:\Windows\System32\cmd.exe /c echo piet)");
            cmdNode->workingDirectory(subDirs[0]);
            context.nodes().add(cmdNode);

            auto cmdNode1 = std::make_shared<CommandNode>(&context, std::filesystem::path("@@repo") / "__cmd1");
            cmdNode1->script(R"(C:\Windows\System32\cmd.exe /c echo piet1)");
            cmdNode1->workingDirectory(sourceFileRepo()->directoryNode());
            cmdNode1->cmdInputs({ cmdNode });
            context.nodes().add(cmdNode1);

            // Also execute the cmdNode and file nodes which are still dirty
            std::vector<std::shared_ptr<Node>> dirtyNodes;
            context.getDirtyNodes(dirtyNodes);
            completed = YAMTest::executeNodes(dirtyNodes);
            EXPECT_EQ(nNodes, context.nodes().size());
            persistentState.store();
        }

        std::shared_ptr<FileRepositoryNode> sourceFileRepo() {
            return context.findRepository(std::string("repo"));
        }

        std::shared_ptr<CommandNode> cmdNode() {
            auto node = context.nodes().find(std::filesystem::path("@@repo") / "__cmd");
            return dynamic_pointer_cast<CommandNode>(node);
        }

        std::shared_ptr<CommandNode> cmd1Node() {
            auto node = context.nodes().find(std::filesystem::path("@@repo") / "__cmd1");
            return dynamic_pointer_cast<CommandNode>(node);
        }

        void retrieve() {
            std::filesystem::path repoSymPath = sourceFileRepo()->symbolicDirectory();
            persistentState.retrieve();
            sourceFileRepo()->startWatching();
        }

        bool consumeFileChangeEvent(std::initializer_list<std::filesystem::path> paths) {
            return consumeFileChangeEvents(sourceFileRepo().get(), context.mainThreadQueue(), paths);
        }

        void updateFile(std::filesystem::path const& fileToUpdate) {
            std::ofstream file(fileToUpdate.string());
            file << "Add some content to the file";
            file.close();
        }

        std::filesystem::path addNode() {
            std::filesystem::path file4(repoDir / "File4");
            auto symFile4 = sourceFileRepo()->symbolicPathOf(file4);
            testTree.addFile(); // File4
            EXPECT_TRUE(consumeFileChangeEvent({ file4 }));
            std::vector<std::shared_ptr<Node>> dirtyNodes;
            context.getDirtyNodes(dirtyNodes);
            EXPECT_TRUE(YAMTest::executeNodes(dirtyNodes));
            EXPECT_NE(nullptr, context.nodes().find(symFile4));
            return file4;
        }

        std::tuple<std::filesystem::path, XXH64_hash_t> modifyNode() {
            std::filesystem::path file3(repoDir / "File3");
            auto symFile3 = sourceFileRepo()->symbolicPathOf(file3);
            auto node = dynamic_pointer_cast<FileNode>(context.nodes().find(symFile3));
            YAMTest::executeNode(node.get());
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
                [](std::shared_ptr<Node> const& n) {
                    n->setState(Node::State::Dirty);
                });
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

        StorageHelper(SetupHelper& setupHelper)
            : setup(setupHelper)
        {
        }

        std::shared_ptr<DirectoryNode> retrieve() {
            setup.retrieve();
            return setup.sourceFileRepo()->directoryNode();
        }

        std::size_t store() {
            return setup.persistentState.store();
        }

        bool consumeFileChangeEvent(std::initializer_list<std::filesystem::path> paths) {
            auto fileRepo = setup.context.findRepository("repo");
            return consumeFileChangeEvents(fileRepo.get(),setup.context.mainThreadQueue(), paths);
        }

        XXH64_hash_t addFileAndUpdateFileAndExecuteNode(std::shared_ptr<FileNode> fileNode) {
            bool completed = YAMTest::executeNode(fileNode.get());
            EXPECT_TRUE(completed);
            auto hash = fileNode->hashOf(FileAspect::entireFileAspect().name());
            setup.testTree.addFile(); // add File4
            setup.updateFile(fileNode->absolutePath());
            bool consumed = consumeFileChangeEvent({ fileNode->absolutePath(), setup.repoDir / "File4" });
            if (!consumed || Node::State::Dirty != fileNode->state()) throw std::exception("wrong state");
            // First execution detects new file without hashing it...
            std::vector<std::shared_ptr<Node>> dirtyNodes;
            setup.context.getDirtyNodes(dirtyNodes);
            completed = YAMTest::executeNodes(dirtyNodes);
            EXPECT_TRUE(completed);
            // Subsequently execute the file to recompute the file hash...
            setup.context.getDirtyNodes(dirtyNodes);
            completed = YAMTest::executeNodes(dirtyNodes);
            EXPECT_TRUE(completed);
            // ... and verify that hash has changed
            auto updatedHash = fileNode->hashOf(FileAspect::entireFileAspect().name());
            EXPECT_NE(hash, updatedHash);
            return updatedHash;
        }
    };

    TEST(PersistentBuildState, storeAddedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        std::filesystem::path addedName("addedNode");
        std::string addedScript("a serious script");
        auto addedNode = std::make_shared<CommandNode>(&setup.context, addedName);
        addedNode->script(addedScript);
        setup.context.nodes().add(addedNode);
        addedNode = nullptr;
        setup.persistentState.store();

        setup.persistentState.retrieve();
        addedNode = dynamic_pointer_cast<CommandNode>(setup.context.nodes().find(addedName));
        ASSERT_NE(nullptr, addedNode);
        EXPECT_EQ(addedScript, addedNode->script());
    }

    TEST(PersistentBuildState, storeModifiedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        auto node = setup.cmdNode();
        auto nodeName = node->name();
        node->script("rubbish");
        setup.persistentState.store();
        node = nullptr;

        setup.persistentState.retrieve();
        node = dynamic_pointer_cast<CommandNode>(setup.context.nodes().find(nodeName));
        ASSERT_NE(nullptr, node);
        EXPECT_EQ("rubbish", node->script());
    }


    TEST(PersistentBuildState, storeModifiedRemovedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        auto node = setup.cmdNode();
        auto nodeName = node->name();
        node->script("rubbish");
        EXPECT_TRUE(node->modified());
        setup.context.nodes().remove(node);
        node = nullptr;
        setup.persistentState.store();

        setup.persistentState.retrieve();
        ASSERT_EQ(nullptr, setup.context.nodes().find(nodeName));
    }


    TEST(PersistentBuildState, storeRemovedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        auto cmd1 = setup.cmd1Node();
        auto cmd1Name = cmd1->name();
        EXPECT_FALSE(cmd1->modified());
        setup.context.nodes().remove(cmd1);
        cmd1 = nullptr;
        setup.persistentState.store(); 
        EXPECT_FALSE(setup.persistentState.isPendingDelete(cmd1Name.string()));

        setup.persistentState.retrieve();
        ASSERT_EQ(nullptr, setup.context.nodes().find(cmd1Name));
    }

    TEST(PersistentBuildState, storedRemovedReferencedCmd) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        //cmdNode() is referenced by cmd1Node()
        auto cmd = setup.cmdNode();
        auto cmdName = cmd->name().string();
        setup.context.nodes().remove(cmd);
        EXPECT_EQ(Node::State::Deleted, cmd->state());
        cmd = nullptr;
        setup.persistentState.store();
        EXPECT_TRUE(setup.persistentState.isPendingDelete(cmdName));

        setup.persistentState.retrieve();
        cmd = setup.cmdNode();
        EXPECT_EQ(nullptr, cmd);
        EXPECT_NE(nullptr, setup.cmd1Node());
        EXPECT_EQ(cmdName, setup.cmd1Node()->cmdInputs()[0]->name());

        auto cmd1 = setup.cmd1Node();
        auto cmd1Name = cmd1->name().string();
        setup.context.nodes().remove(cmd1);
        EXPECT_EQ(Node::State::Deleted, cmd1->state());
        auto cmd1Ptr = cmd1.get();
        cmd1 = nullptr;
        setup.persistentState.store();
        EXPECT_FALSE(setup.persistentState.isPendingDelete(cmd1Name));

        setup.persistentState.retrieve();
        cmd = setup.cmdNode();
        cmd1 = setup.cmd1Node();
        EXPECT_EQ(nullptr, cmd);
        EXPECT_EQ(nullptr, cmd1);
    }


    TEST(PersistentBuildState, storeAndRetrieve) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        // Retrieve the nodes stored by setup.
        StorageHelper storage(setup);
        auto repoDirNode = storage.retrieve();
        ASSERT_NE(nullptr, repoDirNode);
        EXPECT_EQ(nNodes, setup.context.nodes().size());

        auto cmdNode = setup.cmdNode();
        EXPECT_NE(nullptr, cmdNode);
        EXPECT_EQ(cmdNode->workingDirectory()->name(), cmdNode->workingDirectory()->name());
        cmdNode = nullptr;

        // Verify that the retrieved build state can be executed
        bool completed = YAMTest::executeNode(repoDirNode.get());
        EXPECT_TRUE(completed);
        // +1 because buildstate file is now in buildstate dir
        EXPECT_EQ(nNodes, setup.context.nodes().size());
        storage.store(); // executeNode put all nodes to modified state

        // Verify that re-executing a file node sets it modified.
        std::filesystem::path root(setup.sourceFileRepo()->symbolicDirectory());
        auto fileNode = dynamic_pointer_cast<FileNode>(setup.context.nodes().find(root / "File3"));
        ASSERT_NE(nullptr, fileNode);
        auto updatedHash = storage.addFileAndUpdateFileAndExecuteNode(fileNode);
        EXPECT_TRUE(fileNode->modified());
        EXPECT_EQ(nNodes+1, setup.context.nodes().size()); // new FileNode for File4
        auto newFileNode = dynamic_pointer_cast<FileNode>(setup.context.nodes().find(root / "File4"));
        EXPECT_NE(nullptr, newFileNode);
        EXPECT_TRUE(newFileNode->modified());

        // Verify that the modified file node is updated in storage.
        std::size_t nStored = storage.store(); // store the modified file node.
        EXPECT_EQ(3, nStored); // repo dir, file3, file4
        storage.retrieve(); // replace all nodes in storage.context by ones freshly retrieved from storage
        fileNode = dynamic_pointer_cast<FileNode>(setup.context.nodes().find(root / "File3"));
        ASSERT_NE(nullptr, fileNode);

        EXPECT_FALSE(fileNode->modified());
        newFileNode = dynamic_pointer_cast<FileNode>(setup.context.nodes().find(root / "File4"));
        EXPECT_NE(nullptr, newFileNode);
        EXPECT_FALSE(newFileNode->modified());

        auto actualHash = fileNode->hashOf(FileAspect::entireFileAspect().name());
        EXPECT_EQ(updatedHash, actualHash);
    }

    TEST(PersistentBuildState, rollbackRemovedRepo) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        std::string repoName = setup.sourceFileRepo()->repoName();
        EXPECT_TRUE(setup.context.repositoriesNode()->removeRepository(repoName));
        EXPECT_EQ(4, setup.context.nodes().size()); // repository, repositoriesconfigfile, cmd and cmd1 node

        setup.persistentState.rollback();

        EXPECT_EQ(nNodes, setup.context.nodes().size());
        EXPECT_NE(nullptr, setup.context.findRepository(repoName));
        // Verify that the rolled-back build state can be executed
        setup.executeAll();
    }

    TEST(PersistentBuildState, rollbackRemovedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        std::shared_ptr<CommandNode> cmdBeforeRollback = setup.cmd1Node();
        auto cmd1Name = cmdBeforeRollback->name().string();
        std::string scriptBeforeRollback = cmdBeforeRollback->script();
        cmdBeforeRollback->script(R"(rubbish)");
        ASSERT_TRUE(cmdBeforeRollback->modified());
        setup.context.nodes().remove(cmdBeforeRollback);
        setup.persistentState.rollback();

        auto cmdAfterRollback = setup.cmd1Node();
        EXPECT_FALSE(setup.persistentState.isPendingDelete(cmd1Name));
        EXPECT_EQ(scriptBeforeRollback, cmdAfterRollback->script());
        ASSERT_EQ(1, cmdAfterRollback->cmdInputs().size());
        EXPECT_EQ(setup.cmdNode(), cmdAfterRollback->cmdInputs()[0]);
        EXPECT_EQ(nNodes, setup.context.nodes().size());

        // Verify that the rolled-back build state can be executed
        setup.executeAll();
    }

    TEST(PersistentBuildState, rollbackRemoveReferencedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        std::shared_ptr<CommandNode> cmdBeforeRollback = setup.cmdNode();
        auto cmdName = cmdBeforeRollback->name().string();
        std::string scriptBeforeRollback = cmdBeforeRollback->script();
        cmdBeforeRollback->script(R"(rubbish)");
        ASSERT_TRUE(cmdBeforeRollback->modified());
        setup.context.nodes().remove(cmdBeforeRollback);
        setup.persistentState.rollback();

        auto cmdAfterRollback = setup.cmdNode();
        EXPECT_FALSE(setup.persistentState.isPendingDelete(cmdName));
        EXPECT_EQ(scriptBeforeRollback, cmdAfterRollback->script());

        EXPECT_EQ(nNodes, setup.context.nodes().size());
        // Verify that the rolled-back build state can be executed
        setup.executeAll();
    }

    TEST(PersistentBuildState, rollbackAddedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        std::filesystem::path addedFile = setup.addNode();
        EXPECT_EQ(nNodes + 1, setup.context.nodes().size());

        setup.persistentState.rollback();

        auto symAddedFile = setup.sourceFileRepo()->symbolicPathOf(addedFile);
        EXPECT_EQ(nNodes, setup.context.nodes().size());
        EXPECT_EQ(nullptr, setup.context.nodes().find(symAddedFile));
        // Verify that the rolled-back build state can be executed
        setup.executeAll();
    }

    TEST(PersistentBuildState, rollbackModifiedNode) {
        SetupHelper setup(FileSystem::createUniqueDirectory());

        auto tuple = setup.modifyNode();
        std::filesystem::path modifiedFile = std::get<0>(tuple);
        XXH64_hash_t hashBeforeModify = std::get<1>(tuple);

        setup.persistentState.rollback();

        EXPECT_EQ(nNodes, setup.context.nodes().size());
        auto symModifiedFile = setup.sourceFileRepo()->symbolicPathOf(modifiedFile);
        auto node = dynamic_pointer_cast<FileNode>(setup.context.nodes().find(symModifiedFile));
        XXH64_hash_t hash = node->hashOf(FileAspect::entireFileAspect().name());
        EXPECT_EQ(hashBeforeModify, hash);
        // Verify that the rolled-back build state can be executed
        setup.executeAll();
    }


    class SetupHelper1
    {
    public:
        std::filesystem::path repoDir;
        DirectoryTree testTree;
        ExecutionContext context;
        std::shared_ptr<FileRepositoryNode> repo0;
        std::shared_ptr<FileRepositoryNode> repo1;
        PersistentBuildState persistentState;

        SetupHelper1(std::filesystem::path const& repoDirectory)
            : repoDir(repoDirectory)
            , testTree(repoDir, 4, RegexSet({ ".yam" }))
            , persistentState(getBuildStateFile(repoDir), &context)
        {
            context.threadPool().size(1);
            std::filesystem::create_directory(repoDir / "r0");
            std::filesystem::create_directory(repoDir / "r1");
            persistentState.retrieve();
            if (context.repositoriesNode() == nullptr) {
                repo0 = std::make_shared<FileRepositoryNode>(
                    &context,
                    "repo0",
                    repoDir/"r0");
                auto repos = std::make_shared<RepositoriesNode>(&context, repo0);
                context.repositoriesNode(repos);
                repo1 = std::make_shared<FileRepositoryNode>(
                    &context,
                    "repo1",
                    repoDir/"r1");
                repos->addRepository(repo1);

            }
        }
    };

    TEST(PersistentBuildState, addAndRemoveNodes) {
        std::filesystem::path repoDir = FileSystem::createUniqueDirectory();
        DotYamDirectory::create(repoDir);

        for (int i = 0; i < 5; i++) {
            SetupHelper1 setup(repoDir);
            bool completed = YAMTest::executeNodes({
                setup.repo0->directoryNode(),
                setup.repo1->directoryNode()
            });
            ASSERT_TRUE(completed);
            setup.persistentState.store();
            setup.repo0->directoryNode()->clear();
            setup.persistentState.store();
        }
    }
}
