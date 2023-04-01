
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../SourceFileRepository.h"
#include "../SourceDirectoryNode.h"
#include "../FileNode.h"
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

    // repoDir bevat .yam dir, subdirs 1,2,3 en files 1,2,3
    // Elke subdir bevat 39 files en 12 mappen.
    // Inclusief repoDir zelf: 41 mappen, 120 files.
    // Nodes: per directory 4 nodes (dir node, dotignore .yamignore and
    // .gitignore). Per file 1 node, per file repository 1 file.
    // Totaal: 41*4 + 120 + 1 = 285
    const std::size_t nNodes = 284;
    const std::size_t nObjects = nNodes+1;

    // Create directory tree, mirror tree in source directory node and
    // store resulting directory node tree to repoDir/ "nodes"
    class SetupHelper
    {
    public:

        std::filesystem::path repoDir;
        std::filesystem::path yamDir;
        DirectoryTree testTree;
        ExecutionContext context;
        std::shared_ptr<SourceFileRepository> sourceFileRepo;
        std::shared_ptr<SourceDirectoryNode> repoDirNode;

        SetupHelper(std::filesystem::path const& repoDirectory)
            : repoDir(repoDirectory)
            , yamDir(DotYamDirectory::create(repoDir))
            , testTree(repoDir, 3, RegexSet({ ".yam" }))
            , sourceFileRepo(std::make_shared<SourceFileRepository>(
                "repo",
                repoDir,
                RegexSet({ "buildState" }),
                &context))
            , repoDirNode(sourceFileRepo->directoryNode())
        {
            context.addRepository(sourceFileRepo);
            bool completed = YAMTest::executeNode(repoDirNode.get());
            EXPECT_TRUE(completed);
            EXPECT_EQ(nNodes, context.nodes().size());
            std::filesystem::create_directory(repoDir / "buildState");
            PersistentBuildState psetStore(repoDir / "buildState", &context);
            psetStore.store();
        }
    };

    class StorageHelper
    {
    public:
        SetupHelper& setup;
        ExecutionContext context;
        PersistentBuildState storage;
        std::shared_ptr<SourceDirectoryNode> repoDirNode;

        StorageHelper(SetupHelper& setupHelper)
            : setup(setupHelper)
            , storage(setup.repoDir / "buildState", &context)
        {
            context.addRepository(std::make_shared<FileRepository>("repo", setup.repoDir));
        }

        std::shared_ptr<SourceDirectoryNode> retrieve() {
            storage.retrieve();
            return dynamic_pointer_cast<SourceDirectoryNode>(context.nodes().find(setup.repoDir));
        }

        uint32_t store() {
            return storage.store();
        }

        // Wait for file change event to be received for given paths.
        // When event is received then consume the changes.
        // Return whether event was consumed.
        bool consumeFileChangeEvent(std::initializer_list<std::filesystem::path> paths)
        {
            auto sourceFileRepo = dynamic_pointer_cast<SourceFileRepository>(context.findRepository("repo"));
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
                context.mainThreadQueue().push(pollChange);
                dispatcher.run();
                if (!received) std::this_thread::sleep_for(retryInterval);
            } while (nRetries < maxRetries && !received);
            return received;
        }

        XXH64_hash_t addFileAndUpdateFileAndExecuteNode(std::shared_ptr<FileNode> fileNode) {
            setup.testTree.addFile(); // will add File4
            auto hash = fileNode->hashOf(FileAspect::entireFileAspect().name());
            // update the file associated with fileNode...
            std::ofstream file3(fileNode->name().string());
            file3 << "Add some content to the file";
            file3.close();
            bool consumed = consumeFileChangeEvent({ fileNode->name(), setup.repoDir / "File4" });
            if (!consumed || Node::State::Dirty != fileNode->state()) throw std::exception("wrong state");
            // ... execute the file to recompute the file hash...
            std::vector<std::shared_ptr<Node>> dirtyNodes;
            context.getDirtyNodes(dirtyNodes);
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
        EXPECT_EQ(nNodes, storage.context.nodes().size());

        // Verify that the retrieved nodes can be executed
        bool completed = YAMTest::executeNode(repoDirNode.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(nNodes, storage.context.nodes().size());
        storage.store(); // executeNode put all nodes to modified state

        // Verify that re-executing a file node sets it modified.
        auto fileNode = dynamic_pointer_cast<FileNode>(storage.context.nodes().find(setup.repoDir / "File3"));
        ASSERT_NE(nullptr, fileNode);
        auto updatedHash = storage.addFileAndUpdateFileAndExecuteNode(fileNode);
        EXPECT_TRUE(fileNode->modified());
        EXPECT_EQ(nNodes+1, storage.context.nodes().size()); // new FileNode for File4
        auto newFileNode = dynamic_pointer_cast<FileNode>(storage.context.nodes().find(setup.repoDir / "File4"));
        EXPECT_NE(nullptr, newFileNode);
        EXPECT_TRUE(newFileNode->modified());

        // Verify that the modified file node is updated in storage.
        std::size_t nStored = storage.store(); // store the modified file node.
        EXPECT_EQ(3, nStored); // repo dir, file3, file4
        storage.retrieve(); // replace all nodes in storage.context by ones freshly retrieved from storage
        fileNode = dynamic_pointer_cast<FileNode>(storage.context.nodes().find(setup.repoDir / "File3"));
        ASSERT_NE(nullptr, fileNode);

        EXPECT_FALSE(fileNode->modified());
        newFileNode = dynamic_pointer_cast<FileNode>(storage.context.nodes().find(setup.repoDir / "File4"));
        EXPECT_NE(nullptr, newFileNode);
        EXPECT_FALSE(newFileNode->modified());

        auto actualHash = fileNode->hashOf(FileAspect::entireFileAspect().name());
        EXPECT_EQ(updatedHash, actualHash);
    }
}
