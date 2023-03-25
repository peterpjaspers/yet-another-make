
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../FileRepository.h"
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
    // .gitignore. Per file 1 node. Totaal: 41*4 + 120 = 284
    const std::size_t nNodes = 284;

    // Create directory tree, mirror tree in source directory node and
    // store resulting directory node tree to repoDir/ "nodes"
    class SetupHelper
    {
    public:

        std::filesystem::path repoDir;
        std::filesystem::path yamDir;
        DirectoryTree testTree;
        ExecutionContext context;
        std::shared_ptr<SourceDirectoryNode> repoDirNode;


        SetupHelper(std::filesystem::path const& repoDirectory)
            : repoDir(repoDirectory)
            , yamDir(DotYamDirectory::create(repoDir))
            , testTree(repoDir, 3, RegexSet({ ".yam" }))
        {
            context.addRepository(std::make_shared<FileRepository>("repo", repoDir));
            repoDirNode = std::make_shared<SourceDirectoryNode>(&context, repoDir);
            context.nodes().add(repoDirNode);
            bool completed = YAMTest::executeNode(repoDirNode.get());
            EXPECT_TRUE(completed);
            EXPECT_EQ(nNodes, context.nodes().size());
            std::filesystem::create_directory(repoDir / "nodes");
            PersistentBuildState psetStore(repoDir / "nodes", &context);
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
            , storage(setup.repoDir / "nodes", &context)
        {
            context.addRepository(std::make_shared<FileRepository>("repo", setup.repoDir));
        }

        std::shared_ptr<SourceDirectoryNode> retrieve() {
            storage.retrieve();
            return dynamic_pointer_cast<SourceDirectoryNode>(context.nodes().find(setup.repoDir));
        }

        void store() {
            storage.store();
        }

        XXH64_hash_t updateFileAndExecuteNode(std::shared_ptr<FileNode> fileNode) {
            auto hash = fileNode->hashOf(FileAspect::entireFileAspect().name());
            // update the file associated with fileNode...
            std::ofstream file3(fileNode->name().string());
            file3 << "Add some content to the file";
            file3.close();
            fileNode->setState(Node::State::Dirty);
            // ... execute the file to recompute the file hash...
            bool completed = YAMTest::executeNode(fileNode.get());
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

        // Executing repoDirNode now also mirrored the repoDir/nodes dir.
        // This brings total nr of nodes to (2*nNodes) + 24: 
        // 2*nNodes because each node is stored in a file of its own
        // +24 because repoDir/nodes has 5 subdirs, one for each node type.
        // (5 subdirs + nodes dir) = 6*4 nodes = 24
        EXPECT_EQ((2*nNodes) + 24, storage.context.nodes().size());
        storage.store(); // executeNode put all nodes to modified state

        // Verify that re-executing a file node sets it modified.
        auto fileNode = dynamic_pointer_cast<FileNode>(storage.context.nodes().find(setup.repoDir / "File3"));
        ASSERT_NE(nullptr, fileNode);
        auto updatedHash = storage.updateFileAndExecuteNode(fileNode);
        EXPECT_TRUE(fileNode->modified());

        // Verify that the modified file node is updated in storage.
        storage.store(); // store the modified file node.
        storage.retrieve(); // replace all nodes in storage.context by ones freshly retrieved from storage
        fileNode = dynamic_pointer_cast<FileNode>(storage.context.nodes().find(setup.repoDir / "File3"));
        ASSERT_NE(nullptr, fileNode);
        auto actualHash = fileNode->hashOf(FileAspect::entireFileAspect().name());
        EXPECT_EQ(updatedHash, actualHash);
    }
}
