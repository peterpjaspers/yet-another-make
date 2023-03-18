
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../FileRepository.h"
#include "../SourceDirectoryNode.h"
#include "../ExecutionContext.h"
#include "../DotYamDirectory.h"
#include "../PersistentNodeSet.h"
#include "../FileSystem.h"
#include "../RegexSet.h"

#include "gtest/gtest.h"
#include <chrono>
#include <memory>

namespace
{
    using namespace YAM;
    using namespace YAMTest;

    TEST(PersistentNodeSetTest, threeDeepDirectoryTree) {
        std::filesystem::path repoDir = FileSystem::createUniqueDirectory();
        std::filesystem::path yamDir = DotYamDirectory::create(repoDir);

        DirectoryTree testTree(repoDir, 3, RegexSet({ ".yam" }));

        ExecutionContext context;
        context.addRepository(std::make_shared<FileRepository>("repo", repoDir));
        auto repoDirNode = std::make_shared<SourceDirectoryNode>(&context, repoDir);
        bool completed = YAMTest::executeNode(repoDirNode.get());
        EXPECT_TRUE(completed);

        std::filesystem::create_directory(repoDir / "nodes");
        PersistentNodeSet pnodesWrite(repoDir / "nodes", &context);
        NodeSet nodes;
        pnodesWrite.retrieve();
        EXPECT_EQ(0, nodes.size());
        pnodesWrite.insert(repoDirNode);

        ExecutionContext retrieved;
        PersistentNodeSet pnodesRead(repoDir / "nodes", &retrieved);
        pnodesRead.retrieve();
    }
}
