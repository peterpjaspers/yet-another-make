
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../BuildFileParserNode.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../CommandNode.h"
#include "../GlobNode.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../RegexSet.h"

#include "gtest/gtest.h"
#include <chrono>

namespace
{
    using namespace YAM;
    using namespace YAMTest;

    class TestSetup {
    public:
        DirectoryTree repoTree;
        ExecutionContext context;
        std::shared_ptr<FileRepository> fileRepo;
        std::filesystem::path absBuildFilePath;
        std::string rule;
        std::shared_ptr<SourceFileNode> buildFileNode;
        std::shared_ptr<BuildFileParserNode> buildFileParserNode;

        TestSetup(bool syntaxError = false)
            : repoTree(FileSystem::createUniqueDirectory("_buildFileProcessingTest"), 1, RegexSet())
            , fileRepo(std::make_shared<FileRepository>("repo", repoTree.path(), &context))
            , absBuildFilePath(repoTree.path() / R"(buildfile.txt)")
            , buildFileParserNode(std::make_shared<BuildFileParserNode>(&context, "buildFileParserNode"))
        {

            if (syntaxError) {
                rule = R"(: *.cpp >| echo main > main.obj |> %%B.obj)";
            } else {
                rule = R"(: foreach *.cpp |> echo main > main.obj |> %%B.obj)";
            }
            std::ofstream stream(absBuildFilePath.string().c_str());
            EXPECT_TRUE(stream.is_open());
            stream << rule;
            stream.close();

            context.addRepository(fileRepo);
            auto dirNode = fileRepo->directoryNode();
            bool completed = YAMTest::executeNode(dirNode.get());
            EXPECT_TRUE(completed);
            buildFileNode = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(fileRepo->symbolicPathOf(absBuildFilePath)));
            buildFileParserNode->buildFile(buildFileNode);
        }
    };

    TEST(BuildFileParserNode, parse) {
        TestSetup setup;
        std::shared_ptr<BuildFileParserNode> parser = setup.buildFileParserNode;
        EXPECT_EQ(Node::State::Dirty, parser->state());
        bool completed = YAMTest::executeNode(parser.get());
        EXPECT_TRUE(completed);
        ASSERT_EQ(Node::State::Ok, parser->state());
        ASSERT_EQ(1, parser->detectedInputs().size());
        EXPECT_NE(parser->detectedInputs().end(), parser->detectedInputs().find(parser->buildFile()->name()));
        BuildFile::File const& parseTree = parser->parseTree();
        ASSERT_EQ(1, parseTree.variablesAndRules.size());
        auto const& rule = *dynamic_pointer_cast<BuildFile::Rule>(parseTree.variablesAndRules[0]);
        EXPECT_TRUE(rule.forEach);
        EXPECT_EQ(" echo main > main.obj ", rule.script.script);
    }

    TEST(BuildFileParserNode, parseError) {
        TestSetup setup(true);
        std::shared_ptr<BuildFileParserNode> parser = setup.buildFileParserNode;
        EXPECT_EQ(Node::State::Dirty, parser->state());
        bool completed = YAMTest::executeNode(parser.get());
        EXPECT_TRUE(completed);
        ASSERT_EQ(Node::State::Failed, parser->state());
        EXPECT_ANY_THROW(parser->parseTree());
    }
    TEST(BuildFileParserNode, reParse) {
        TestSetup setup;
        std::shared_ptr<BuildFileParserNode> parser = setup.buildFileParserNode;
        EXPECT_EQ(Node::State::Dirty, parser->state());
        bool completed = YAMTest::executeNode(parser.get());
        EXPECT_TRUE(completed);
        ASSERT_EQ(Node::State::Ok, parser->state());

        // update buildfile
        std::ofstream stream(setup.absBuildFilePath.string().c_str());
        EXPECT_TRUE(stream.is_open());
        stream << R"(// this is comment
: *.cpp |> type main > main.obj |> %B.obj)";
        stream.close();
        // Wait until file change events have resulted in processingNode to
        // become dirty. Take care: node states are updated in main thread.
        // Hence getting node state in test thread is not reliable. Therefore
        // retrieve node state in main thread.
        Dispatcher dispatcher;
        bool dirty = false;
        auto fillDirtyNodes = Delegate<void>::CreateLambda(
            [&]() {
            setup.fileRepo->consumeChanges();
        dirty = parser->state() == Node::State::Dirty;
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
        completed = YAMTest::executeNode(parser.get());
        EXPECT_TRUE(completed);
        ASSERT_EQ(Node::State::Ok, parser->state());
        BuildFile::File const& parseTree = parser->parseTree();
        ASSERT_EQ(1, parseTree.variablesAndRules.size());
        auto const& rule = *dynamic_pointer_cast<BuildFile::Rule>(parseTree.variablesAndRules[0]);
        EXPECT_FALSE(rule.forEach);
        EXPECT_EQ(" type main > main.obj ", rule.script.script);

        EXPECT_EQ(2, setup.context.statistics().nSelfExecuted);
        auto const& selfExecuted = setup.context.statistics().selfExecuted;
        EXPECT_NE(selfExecuted.end(), selfExecuted.find(parser.get()));
        EXPECT_NE(selfExecuted.end(), selfExecuted.find(setup.buildFileNode.get()));
    }

    TEST(BuildFileParserNode, noReParse) {
        TestSetup setup;
        std::shared_ptr<BuildFileParserNode> parser = setup.buildFileParserNode;
        EXPECT_EQ(Node::State::Dirty, parser->state());
        bool completed = YAMTest::executeNode(parser.get());
        EXPECT_TRUE(completed);
        ASSERT_EQ(Node::State::Ok, parser->state());

        // update buildfile with same content => same hash => no reparse
        std::ofstream stream(setup.absBuildFilePath.string().c_str());
        EXPECT_TRUE(stream.is_open());
        stream << setup.rule;
        stream.close();

        // Wait until file change events have resulted in processingNode to
        // become dirty. Take care: node states are updated in main thread.
        // Hence getting node state in test thread is not reliable. Therefore
        // retrieve node state in main thread.
        Dispatcher dispatcher;
        bool dirty = false;
        auto fillDirtyNodes = Delegate<void>::CreateLambda(
            [&]() {
            setup.fileRepo->consumeChanges();
        dirty = parser->state() == Node::State::Dirty;
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
        completed = YAMTest::executeNode(parser.get());
        EXPECT_TRUE(completed);
        ASSERT_EQ(Node::State::Ok, parser->state());
        BuildFile::File const& parseTree = parser->parseTree();
        ASSERT_EQ(1, parseTree.variablesAndRules.size());
        auto const& rule = *dynamic_pointer_cast<BuildFile::Rule>(parseTree.variablesAndRules[0]);
        EXPECT_TRUE(rule.forEach);
        EXPECT_EQ(" echo main > main.obj ", rule.script.script);

        EXPECT_EQ(1, setup.context.statistics().nSelfExecuted);
        auto const& selfExecuted = setup.context.statistics().selfExecuted;
        EXPECT_NE(selfExecuted.end(), selfExecuted.find(setup.buildFileNode.get()));
        EXPECT_EQ(1, setup.context.statistics().nRehashedFiles);
        auto const& rehashed = setup.context.statistics().rehashedFiles;
        EXPECT_NE(rehashed.end(), rehashed.find(setup.buildFileNode.get()));
    }

}