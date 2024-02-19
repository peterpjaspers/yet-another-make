
#include "executeNode.h"
#include "DirectoryTree.h"
#include "../BuildFileParserNode.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../GeneratedFileNode.h"
#include "../CommandNode.h"
#include "../GlobNode.h"
#include "../RepositoriesNode.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../RegexSet.h"
#include "../AcyclicTrail.h"

#include "gtest/gtest.h"
#include <chrono>

namespace
{
    using namespace YAM;
    using namespace YAMTest;

    void writeFile(std::filesystem::path p, std::string const& content) {
        std::ofstream stream(p.string().c_str());
        EXPECT_TRUE(stream.is_open());
        stream << content;
        stream.close();
    }

    class TestSetup {
    public:
        DirectoryTree repoTree;
        ExecutionContext context;
        std::shared_ptr<FileRepository> fileRepo;
        std::filesystem::path absBuildFilePath;
        std::filesystem::path absBuildFilePathSD1;
        std::filesystem::path absBuildFilePathSD2;
        std::string rule;
        std::string ruleSD1;
        std::string ruleSD2;
        std::shared_ptr<SourceFileNode> buildFileNode;
        std::shared_ptr<BuildFileParserNode> buildFileParserNode;
        std::shared_ptr<BuildFileParserNode> buildFileParserNodeSD1;
        std::shared_ptr<BuildFileParserNode> buildFileParserNodeSD2;

        TestSetup(bool syntaxError = false)
            : repoTree(FileSystem::createUniqueDirectory("_buildFileProcessingTest"), 1, RegexSet())
            , fileRepo(std::make_shared<FileRepository>("repo", repoTree.path(), &context, true))
            , absBuildFilePath(repoTree.path() / R"(buildfile_yam.bat)")
            , absBuildFilePathSD1(repoTree.path() / R"(SubDir1\buildfile_yam.txt)")
            , absBuildFilePathSD2(repoTree.path() / R"(SubDir2\buildfile_yam.txt)")
        {
            if (syntaxError) {
                std::stringstream ss;
                ss
                    << "@echo off" << std::endl
                    << R"(echo : buildfile SubDir1 buildfile SubDir2 : foreach *.cpp ^|^> echo main ^> main.obj ^|^> %%B.obj)"
                    << std::endl;
                rule = ss.str();
                ruleSD1 = rule;
                ruleSD2 = rule;
            } else {
                std::stringstream ss;
                ss
                    << "@echo off" << std::endl
                    << R"(echo buildfile SubDir1 buildfile SubDir2 : foreach *.cpp ^|^> echo main ^> main.obj ^|^> %%B.obj)"
                    << std::endl;
                rule = ss.str();
                //rule = R"(buildfile SubDir1 buildfile SubDir2 : foreach *.cpp |> echo main > main.obj |> %%B.obj)";
                ruleSD1 = R"(buildfile .. : foreach *.cpp |> echo main > main.obj |> %%B.obj )";
                ruleSD2 = R"(buildfile ..\SubDir1 : foreach *.cpp |> echo main > main.obj |> %%B.obj )";
            }
            writeFile(absBuildFilePath, rule);
            writeFile(absBuildFilePathSD1, ruleSD1);
            writeFile(absBuildFilePathSD2, ruleSD2);

            auto repos = std::make_shared<RepositoriesNode>(&context, fileRepo);
            context.repositoriesNode(repos);
            fileRepo->startWatching();

            auto dirNode = fileRepo->directoryNode();
            bool completed = YAMTest::executeNode(dirNode.get());
            EXPECT_TRUE(completed);
            buildFileNode = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(fileRepo->symbolicPathOf(absBuildFilePath)));
            buildFileParserNode = dirNode->buildFileParserNode();
            auto sd1Dir = dynamic_pointer_cast<DirectoryNode>(dirNode->findChild("SubDir1"));
            buildFileParserNodeSD1 = sd1Dir->buildFileParserNode();
            auto sd2Dir = dynamic_pointer_cast<DirectoryNode>(dirNode->findChild("SubDir2"));
            buildFileParserNodeSD2 = sd2Dir->buildFileParserNode();
            EXPECT_NE(nullptr, buildFileParserNode);
            EXPECT_NE(nullptr, buildFileParserNodeSD1);
            EXPECT_NE(nullptr, buildFileParserNodeSD2);
        }
    };

    TEST(BuildFileParserNode, parse) {
        TestSetup setup;
        std::shared_ptr<BuildFileParserNode> parser = setup.buildFileParserNode;
        EXPECT_EQ(Node::State::Dirty, parser->state());
        EXPECT_EQ(Node::State::Dirty, setup.buildFileParserNodeSD1->state());
        EXPECT_EQ(Node::State::Dirty, setup.buildFileParserNodeSD2->state());
        bool completed = YAMTest::executeNodes({ parser.get(), setup.buildFileParserNodeSD1.get(), setup.buildFileParserNodeSD2.get() });
        EXPECT_TRUE(completed);

        ASSERT_EQ(Node::State::Ok, parser->state());
        ASSERT_EQ(Node::State::Ok, setup.buildFileParserNodeSD1->state());
        ASSERT_EQ(Node::State::Ok, setup.buildFileParserNodeSD2->state());

        BuildFile::File const& parseTree = parser->parseTree();
        ASSERT_EQ(1, parseTree.variablesAndRules.size());
        auto const& rule = *dynamic_pointer_cast<BuildFile::Rule>(parseTree.variablesAndRules[0]);
        EXPECT_TRUE(rule.forEach);
        EXPECT_EQ(" echo main > main.obj ", rule.script.script);

        auto dependencies = parser->dependencies();
        ASSERT_EQ(2, dependencies.size());
        EXPECT_EQ(setup.buildFileParserNodeSD1.get(), dependencies[0]);
        EXPECT_EQ(setup.buildFileParserNodeSD2.get(), dependencies[1]);

        auto dependenciesSD1 = setup.buildFileParserNodeSD1->dependencies();
        ASSERT_EQ(1, dependenciesSD1.size());
        auto dependenciesSD2 = setup.buildFileParserNodeSD2->dependencies();
        ASSERT_EQ(1, dependenciesSD2.size());
    }

    TEST(BuildFileParserNode, detectCycles) {
        TestSetup setup;
        std::shared_ptr<BuildFileParserNode> parser = setup.buildFileParserNode;
        bool completed = YAMTest::executeNodes({ parser.get(), setup.buildFileParserNodeSD1.get(), setup.buildFileParserNodeSD2.get() });
        EXPECT_TRUE(completed);

        AcyclicTrail<const BuildFileParserNode*> parserTrail;
        bool notCycling = parser->walkDependencies(parserTrail);
        EXPECT_FALSE(notCycling);
        std::list<const BuildFileParserNode*>const& parseCycle = parserTrail.trail();
        EXPECT_EQ(parser.get(), *(parseCycle.begin()));
        EXPECT_EQ(setup.buildFileParserNodeSD1.get(), *(++parseCycle.begin()));

        AcyclicTrail<const BuildFileParserNode*> parserSD2Trail;
        bool notCyclingSD2 = setup.buildFileParserNodeSD2->walkDependencies(parserSD2Trail);
        EXPECT_FALSE(notCyclingSD2);
        std::list<const BuildFileParserNode*>const& parseSD2Cycle = parserSD2Trail.trail();
        EXPECT_EQ(setup.buildFileParserNodeSD2.get(), *(parseSD2Cycle.begin()));
        EXPECT_EQ(setup.buildFileParserNodeSD1.get(), *(++parseSD2Cycle.begin()));
        EXPECT_EQ(parser.get(), *(++(++parseSD2Cycle.begin())));
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
        std::stringstream ss;
        ss
            << "@echo off" << std::endl
            << R"(echo : *.cpp ^|^> type main ^> main.obj ^|^> %%B.obj)"
            << std::endl;
        writeFile(setup.absBuildFilePath, ss.str());

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

        EXPECT_EQ(5, setup.context.statistics().nSelfExecuted); // buildfile_yam_gen.txt rehashes twice
        auto const& selfExecuted = setup.context.statistics().selfExecuted;
        EXPECT_NE(selfExecuted.end(), selfExecuted.find(setup.buildFileParserNode.get()));
        EXPECT_NE(selfExecuted.end(), selfExecuted.find(setup.buildFileNode.get()));
        auto executor = setup.buildFileParserNode->executor();
        auto const& genBuildFile = executor->outputs()[0];
        EXPECT_NE(selfExecuted.end(), selfExecuted.find(executor.get()));
        EXPECT_NE(selfExecuted.end(), selfExecuted.find(genBuildFile.get()));
        EXPECT_EQ(2, setup.context.statistics().nRehashedFiles);
        auto const& rehashed = setup.context.statistics().rehashedFiles;
        EXPECT_NE(rehashed.end(), rehashed.find(setup.buildFileNode.get()));
        EXPECT_NE(rehashed.end(), rehashed.find(genBuildFile.get()));
    }

    TEST(BuildFileParserNode, noReParse) {
        TestSetup setup;
        std::shared_ptr<BuildFileParserNode> parser = setup.buildFileParserNode;
        EXPECT_EQ(Node::State::Dirty, parser->state());
        bool completed = YAMTest::executeNode(parser.get());
        EXPECT_TRUE(completed);
        ASSERT_EQ(Node::State::Ok, parser->state());

        // update buildfile with same content => same hash => no reparse
        writeFile(setup.absBuildFilePath, setup.rule);

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

        // buildfile_yam.bat and buildfile_yam_gen.txt
        EXPECT_EQ(2, setup.context.statistics().nSelfExecuted);
        auto const& selfExecuted = setup.context.statistics().selfExecuted;
        EXPECT_NE(selfExecuted.end(), selfExecuted.find(setup.buildFileNode.get()));
        EXPECT_EQ(1, setup.context.statistics().nRehashedFiles);
        auto const& rehashed = setup.context.statistics().rehashedFiles;
        EXPECT_NE(rehashed.end(), rehashed.find(setup.buildFileNode.get()));
    }

}