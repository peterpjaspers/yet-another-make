
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../BuildFileParserNode.h"
#include "../BuildFileCompilerNode.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../CommandNode.h"
#include "../GlobNode.h"
#include "../GroupNode.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../RegexSet.h"

#include "gtest/gtest.h"
#include <chrono>

namespace
{
    using namespace YAM;
    using namespace YAMTest;

    void writeFile(std::filesystem::path const& p, std::string const& content) {
        std::ofstream stream(p.string().c_str());
        stream << content;
        stream.close();
    }

    class TestSetup {
    public:
        DirectoryTree repoTree;
        ExecutionContext context;
        std::shared_ptr<FileRepository> fileRepo;
        std::filesystem::path absBuildFilePath;
        std::filesystem::path absSub1BuildFilePath;
        std::filesystem::path cmdOutputFile;
        std::shared_ptr<BuildFileCompilerNode> cCompiler;
        std::shared_ptr<BuildFileCompilerNode> lCompiler;
        std::shared_ptr<CommandNode> cCommand;
        std::shared_ptr<CommandNode> lCommand;
        std::shared_ptr<SourceFileNode> mainFile;

        TestSetup()
            : repoTree(FileSystem::createUniqueDirectory("_buildFileCompilingTest"), 1, RegexSet())
            , fileRepo(std::make_shared<FileRepository>("repo", repoTree.path(), &context))
            , absBuildFilePath(repoTree.path() / R"(buildfile_yam.bat)")
            , absSub1BuildFilePath(repoTree.path() / R"(SubDir1\buildfile_yam.bat)")
            , cmdOutputFile(fileRepo->directoryNode()->name() / "main.obj")
        {
            std::stringstream sscompile;
            sscompile
                << "@echo off" << std::endl
                << R"(echo : foreach *.cpp ^|^> echo %%f ^> %%o ^|^> %%B.obj { objectFiles })"
                << std::endl;
            writeFile(absBuildFilePath, sscompile.str());
            std::stringstream sslink;
            sslink
                << "@echo off" << std::endl
                << R"(echo buildfile ..)" << std::endl
                << R"(echo : { objectFiles } ^|^> echo %%f ^> %%o ^|^> main.exe )"
                << std::endl;
            writeFile(absSub1BuildFilePath, sslink.str());

            std::filesystem::path f1(repoTree.path() / "main.cpp");
            writeFile(f1, "void main() {}");

            context.addRepository(fileRepo);
            auto dirNode = fileRepo->directoryNode();
            bool completed = YAMTest::executeNode(dirNode.get());
            EXPECT_TRUE(completed);

            cCompiler = dirNode->buildFileCompilerNode();
            lCompiler = dynamic_pointer_cast<DirectoryNode>(dirNode->findChild("SubDir1"))->buildFileCompilerNode();
            auto cParser = cCompiler->buildFileParser();
            auto lParser = lCompiler->buildFileParser();

            completed = YAMTest::executeNodes({ cParser, lParser });
            EXPECT_TRUE(completed);
            EXPECT_EQ(Node::State::Ok, cParser->state());
            EXPECT_EQ(Node::State::Ok, lParser->state());

            completed = YAMTest::executeNodes({ cCompiler, lCompiler });
            EXPECT_TRUE(completed);
            EXPECT_EQ(Node::State::Ok, cCompiler->state());
            EXPECT_EQ(Node::State::Ok, lCompiler->state());

            std::filesystem::path cmdName = fileRepo->symbolicDirectory() / "main.obj\\__cmd";
            cCommand = dynamic_pointer_cast<CommandNode>(context.nodes().find(cmdName));
            cmdName = fileRepo->symbolicDirectory() / "SubDir1\\main.exe\\__cmd";
            lCommand = dynamic_pointer_cast<CommandNode>(context.nodes().find(cmdName));

            mainFile = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(fileRepo->symbolicPathOf(f1)));
        }
    };

    TEST(BuildFileCompilerNode, execute) {
        TestSetup setup;
        ASSERT_NE(nullptr, setup.cCommand);
        ASSERT_NE(nullptr, setup.lCommand);
        {
            std::shared_ptr<Node> node = setup.context.nodes().find("objectFiles");
            ASSERT_NE(nullptr, node);
            auto groupNode = dynamic_pointer_cast<GroupNode>(node);
            ASSERT_NE(nullptr, groupNode);
            EXPECT_EQ(Node::State::Dirty, groupNode->state());
            ASSERT_EQ(1, groupNode->group().size());
            EXPECT_EQ(setup.repoTree.path() / "main.obj", groupNode->group()[0]->absolutePath());
        }
        {
            std::filesystem::path compileCmdName = setup.fileRepo->symbolicDirectory() / "main.obj\\__cmd";
            std::shared_ptr<Node> node = setup.context.nodes().find(compileCmdName);
            ASSERT_NE(nullptr, node);
            auto compileNode = dynamic_pointer_cast<CommandNode>(node);
            ASSERT_NE(nullptr, compileNode);
            EXPECT_EQ(Node::State::Dirty, compileNode->state());

            std::filesystem::path linkCmdName = setup.fileRepo->symbolicDirectory() / "SubDir1\\main.exe\\__cmd";
            node = setup.context.nodes().find(linkCmdName);
            ASSERT_NE(nullptr, node);
            auto linkNode = dynamic_pointer_cast<CommandNode>(node);
            ASSERT_NE(nullptr, linkNode);
            EXPECT_EQ(Node::State::Dirty, linkNode->state());

            bool completed = YAMTest::executeNode(linkNode.get());
            EXPECT_TRUE(completed);
            EXPECT_EQ(Node::State::Ok, compileNode->state());
            EXPECT_TRUE(std::filesystem::exists(setup.repoTree.path() / "main.obj"));
            EXPECT_EQ(Node::State::Ok, linkNode->state());
            EXPECT_TRUE(std::filesystem::exists(setup.repoTree.path() / "SubDir1\\main.exe"));
        }
    }

    TEST(BuildFileCompilerNode, reExecuteAfterGlobChange) {
        TestSetup setup;

        std::filesystem::path f1(setup.repoTree.path() / "lib.cpp");
        writeFile(f1, "void lib() {}");

        // Wait until file change events have resulted in compilerNode to
        // become dirty. Take care: node states are updated in main thread.
        // Hence getting node state in test thread is not reliable. Therefore
        // retrieve node state in main thread.
        Dispatcher dispatcher;
        bool dirty = false;
        auto fillDirtyNodes = Delegate<void>::CreateLambda(
            [&]() {
            setup.fileRepo->consumeChanges();
            dirty = setup.cCompiler->state() == Node::State::Dirty;
            dispatcher.stop();
        });
        const auto retryInterval = std::chrono::milliseconds(1000);
        const unsigned int maxRetries = 5;
        unsigned int nRetries = 0;
        do {
            nRetries++;
            dispatcher.start();
            setup.context.mainThreadQueue().push(fillDirtyNodes);
            // dispatcher.run() will block until main thread executed fillDirtyNodes.
            dispatcher.run();
            if (!dirty) std::this_thread::sleep_for(retryInterval);
        } while (nRetries < maxRetries && !dirty);
        ASSERT_TRUE(dirty);
        
        setup.context.statistics().reset();
        setup.context.statistics().registerNodes = true;
        bool completed = YAMTest::executeNode(setup.lCompiler.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, setup.lCompiler->state());
        auto const& selfExecuted = setup.context.statistics().selfExecuted;
        EXPECT_EQ(5, selfExecuted.size());
        auto const& notFound = selfExecuted.end();
        EXPECT_NE(notFound, selfExecuted.find(setup.fileRepo->directoryNode().get()));
        EXPECT_NE(notFound, selfExecuted.find(setup.fileRepo->directoryNode()->findChild("SubDir1").get()));
        EXPECT_NE(notFound, selfExecuted.find(setup.cCompiler.get()));
        EXPECT_NE(notFound, selfExecuted.find(setup.lCompiler.get()));
        auto globName = setup.fileRepo->directoryNode()->name() / "*.cpp";
        auto globNode = setup.context.nodes().find(globName);
        EXPECT_NE(notFound, selfExecuted.find(globNode.get()));
    }

    TEST(BuildFileCompilerNode, noReExecuteAfterDirtyWithoutChanges) {
        TestSetup setup;

        setup.cCompiler->setState(Node::State::Dirty);
        setup.lCompiler->setState(Node::State::Dirty);

        setup.context.statistics().registerNodes = true;
        bool completed = YAMTest::executeNode(setup.lCompiler.get());
        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, setup.cCompiler->state());
        EXPECT_EQ(Node::State::Ok, setup.lCompiler->state());
        setup.context.statistics().reset();
        EXPECT_EQ(0, setup.context.statistics().nSelfExecuted);
    }
}