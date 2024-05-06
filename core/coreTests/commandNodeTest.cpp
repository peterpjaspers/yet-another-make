#include "executeNode.h"
#include "../CommandNode.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../GeneratedFileNode.h"
#include "../GroupNode.h"
#include "../RepositoriesNode.h"
#include "../ThreadPool.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../MultiwayLogBook.h"
#include "../MemoryLogBook.h"
#include "../BasicOStreamLogBook.h"
#include "../FileRepositoryNode.h"

#include "gtest/gtest.h"
#include <boost/process.hpp>
#include <stdlib.h>
#include <fstream>

namespace
{
    using namespace YAMTest;
    using namespace YAM;

    std::string readFile(std::filesystem::path path) {
        std::ifstream file(path.string());
        std::string line, content;
        while (getline(file, line)) content += line;
        file.close();
        return content;        
    }

    class Commands
    {
    public:
        std::filesystem::path repoDir;
        std::shared_ptr<MemoryLogBook> memLogBook;
        std::shared_ptr<BasicOStreamLogBook> stdoutLogBook;
        std::shared_ptr<MultiwayLogBook> logBook;
        ExecutionContext context;
        std::shared_ptr<GroupNode> group;
        std::shared_ptr<CommandNode> pietCmd;
        std::shared_ptr<CommandNode> janCmd;
        std::shared_ptr<CommandNode> pietjanCmd;
        std::shared_ptr<GeneratedFileNode> pietOut;
        std::shared_ptr<GeneratedFileNode> janOut;
        std::shared_ptr<GeneratedFileNode> pietjanOut;
        std::shared_ptr<SourceFileNode> pietSrc;
        std::shared_ptr<SourceFileNode> janSrc;
        ExecutionStatistics& stats;

        Commands()
            : repoDir(YAM::FileSystem::createUniqueDirectory())
            , memLogBook(std::make_shared<MemoryLogBook>())
            , stdoutLogBook(std::make_shared<BasicOStreamLogBook>(&std::cout))
            , logBook(std::make_shared<MultiwayLogBook>())
            , group(std::make_shared<GroupNode>(&context, "<group>"))
            , pietCmd(std::make_shared<CommandNode>(&context, "piet\\_cmd"))
            , janCmd(std::make_shared<CommandNode>(&context, "jan\\_cmd"))
            , pietjanCmd(std::make_shared<CommandNode>(& context, "pietjan\\_cmd"))
            , pietOut(std::make_shared<GeneratedFileNode>(& context, R"(@@.\generated\pietout.txt)", pietCmd))
            , janOut(std::make_shared<GeneratedFileNode>(&context, R"(@@.\generated\janout.txt)", janCmd))
            , pietjanOut(std::make_shared<GeneratedFileNode>(&context, R"(@@.\generated\pietjanout.txt)", pietjanCmd))
            , pietSrc(std::make_shared<SourceFileNode>(&context, R"(@@.\pietsrc.txt)"))
            , janSrc(std::make_shared<SourceFileNode>(&context, R"(@@.\jansrc.txt)"))
            , stats(context.statistics())
        {
            std::filesystem::create_directories(repoDir / "generated");
            logBook->aspects({ LogRecord::Aspect::Error });
            logBook->add(memLogBook);
            logBook->add(stdoutLogBook);
            context.logBook(logBook);

            auto homeRepo =
                std::make_shared<FileRepositoryNode>(
                    &context,
                    ".",
                    repoDir);
            auto repos = std::make_shared<RepositoriesNode>(&context, homeRepo);
            context.repositoriesNode(repos);

            auto winRepo =
                std::make_shared<FileRepositoryNode>(
                    &context,
                    "windows",
                    std::filesystem::path("C:\\Windows"));
            repos->addRepository(winRepo);
            winRepo->repoType(FileRepositoryNode::RepoType::Ignore);

            stats.registerNodes = true;
            //context.threadPool().size(1); // to ease debugging

            std::ofstream pietSrcFile(pietSrc->absolutePath().string());
            pietSrcFile << "piet";
            pietSrcFile.close();
            std::ofstream janSrcFile(janSrc->absolutePath().string());
            janSrcFile << "jan";
            janSrcFile.close();

            {
                std::stringstream script;
                script << "type " << pietSrc->absolutePath().string() << " > " << pietOut->absolutePath().string();
                pietCmd->workingDirectory(homeRepo->directoryNode());
                pietCmd->mandatoryOutputs({ pietOut });
                pietCmd->script(script.str());
            }
            {
                std::stringstream script;
                script << "type " << janSrc->absolutePath().string() << " > " << janOut->absolutePath().string();
                janCmd->workingDirectory(homeRepo->directoryNode());
                janCmd->mandatoryOutputs({ janOut });
                janCmd->script(script.str());
            }
            {
                pietjanCmd->workingDirectory(homeRepo->directoryNode());
                CommandNode::OutputFilter f1(CommandNode::OutputFilter::Type::Ignore, "@@.\\generated\\ignore1.txt");
                CommandNode::OutputFilter f2(CommandNode::OutputFilter::Type::Ignore, "@@.\\**\\ignore2.txt");
                CommandNode::OutputFilter f3(CommandNode::OutputFilter::Type::Optional, "@@.\\generated\\optional[12].txt");
                CommandNode::OutputFilter f4(CommandNode::OutputFilter::Type::Output, "@@.\\generated\\pietjanout.txt");
                pietjanCmd->outputFilters({f1,f2,f3,f4},  { pietjanOut });
                group->add(pietOut);
                pietjanCmd->orderOnlyInputs({ group, janOut });
                //pietjanCmd->orderOnlyInputs({ pietOut, janOut });
                std::stringstream script;
                script
                    << "type " << pietOut->absolutePath().string() << " > " << pietjanOut->absolutePath().string() << std::endl
                    << "type " << janOut->absolutePath().string() << " >> " << pietjanOut->absolutePath().string() << std::endl
                    << "echo optional1 > " << pietjanOut->absolutePath().parent_path() / "optional1.txt" << std::endl
                    << "echo optional2 > " << pietjanOut->absolutePath().parent_path() / "optional2.txt" << std::endl
                    << "echo ignore1 > " << pietjanOut->absolutePath().parent_path() / "ignore1.txt" << std::endl
                    << "echo ignore2 > " << pietjanOut->absolutePath().parent_path() / "ignore2.txt" << std::endl;

                pietjanCmd->script(script.str());
            }

            context.nodes().add(pietCmd);
            context.nodes().add(janCmd);
            context.nodes().add(pietjanCmd);
            context.nodes().add(pietOut);
            context.nodes().add(janOut);
            context.nodes().add(pietjanOut);
            context.nodes().add(pietSrc);
            context.nodes().add(janSrc);

            EXPECT_EQ(Node::State::Dirty, pietCmd->state());
            EXPECT_EQ(Node::State::Dirty, janCmd->state());
            EXPECT_EQ(Node::State::Dirty, pietjanCmd->state());
            EXPECT_EQ(Node::State::Dirty, pietOut->state());
            EXPECT_EQ(Node::State::Dirty, janOut->state());
            EXPECT_EQ(Node::State::Dirty, pietjanOut->state());
            EXPECT_EQ(Node::State::Dirty, pietSrc->state());
            EXPECT_EQ(Node::State::Dirty, janSrc->state());
        }

        ~Commands() {
            clean();
        }

        void clean() {
            context.nodes().remove(pietCmd);
            context.nodes().remove(janCmd);
            context.nodes().remove(pietjanCmd);

            DirectoryNode::removeGeneratedFile(pietOut);
            DirectoryNode::removeGeneratedFile(janOut);
            DirectoryNode::removeGeneratedFile(pietjanOut);
            context.nodes().remove(pietOut);
            context.nodes().remove(janOut);
            context.nodes().remove(pietjanOut);

            context.nodes().remove(pietSrc);
            context.nodes().remove(janSrc);

            context.repositoriesNode()->removeRepository(".");
            std::filesystem::remove_all(repoDir);
        }

        bool execute(bool addSources) {
            std::vector<Node*> dirtyNodes;
            if (addSources) {
                if (pietSrc->state() == Node::State::Dirty) dirtyNodes.push_back(pietSrc.get());
                if (janSrc->state() == Node::State::Dirty) dirtyNodes.push_back(janSrc.get());
            }
            if (pietCmd->state() == Node::State::Dirty) dirtyNodes.push_back(pietCmd.get());
            if (janCmd->state() == Node::State::Dirty) dirtyNodes.push_back(janCmd.get());
            if (pietjanCmd->state() == Node::State::Dirty) dirtyNodes.push_back(pietjanCmd.get());
            stats.reset();
            return executeNodes(dirtyNodes);
        }

        bool execute() {
            return execute(true);
        }
    };

    TEST(CommandNode, cleanBuild) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Ok, cmds.pietCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.janCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.janOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanOut->state());

        std::vector<std::shared_ptr<Node>> inputs;
        cmds.pietCmd->getInputs(inputs);
        ASSERT_EQ(1, inputs.size());
        EXPECT_EQ(cmds.pietSrc, inputs[0]);
        inputs.clear();
        cmds.janCmd->getInputs(inputs);
        ASSERT_EQ(1, inputs.size());
        EXPECT_EQ(cmds.janSrc, inputs[0]);
        CommandNode::OutputNodes const &optionalOutputs = cmds.pietjanCmd->detectedOptionalOutputs();
        auto const& optional1 = optionalOutputs.find("@@.\\generated\\optional1.txt")->second;
        auto const& optional2 = optionalOutputs.find("@@.\\generated\\optional2.txt")->second;

        EXPECT_EQ(11, cmds.stats.started.size());
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.group.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(optional1.get()));
        EXPECT_TRUE(cmds.stats.started.contains(optional2.get()));

        EXPECT_EQ(11, cmds.stats.selfExecuted.size());
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.group.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(optional1.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(optional2.get()));
            
        EXPECT_EQ("piet", readFile(cmds.pietOut->absolutePath()));
        EXPECT_EQ("jan", readFile(cmds.janOut->absolutePath()));
        EXPECT_EQ("pietjan", readFile(cmds.pietjanOut->absolutePath()));
        EXPECT_EQ("optional1 ", readFile(optional1->absolutePath()));
        EXPECT_EQ("optional2 ", readFile(optional2->absolutePath()));
        EXPECT_FALSE(std::filesystem::exists(cmds.pietjanOut->name().parent_path() / "ignore1.txt"));
        EXPECT_FALSE(std::filesystem::exists(cmds.pietjanOut->name().parent_path() / "ignore2.txt"));
    }

    TEST(CommandNode, cleanBuildNoSources) {
        Commands cmds;

        // Remove source file nodes from buildstate to force command
        // nodes to create these input file nodes during command 
        // execution.
        cmds.context.nodes().remove(cmds.pietSrc);
        cmds.context.nodes().remove(cmds.janSrc);

        EXPECT_TRUE(cmds.execute(false));
        EXPECT_EQ(Node::State::Ok, cmds.pietCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.janCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.janOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanOut->state());
        auto pietSrc = cmds.context.nodes().find(cmds.pietSrc->name());
        auto janSrc = cmds.context.nodes().find(cmds.janSrc->name());
        ASSERT_NE(nullptr, pietSrc);
        ASSERT_NE(nullptr, janSrc);
        EXPECT_EQ(Node::State::Ok, pietSrc->state());
        EXPECT_EQ(Node::State::Ok, janSrc->state());

        std::vector<std::shared_ptr<Node>> inputs;
        cmds.pietCmd->getInputs(inputs);
        ASSERT_EQ(1, inputs.size());
        EXPECT_EQ(cmds.pietSrc->name(), inputs[0]->name());
        inputs.clear();
        cmds.janCmd->getInputs(inputs);
        ASSERT_EQ(1, inputs.size());
        EXPECT_EQ(cmds.janSrc->name(), inputs[0]->name());

        // +2 for the optional outputs
        EXPECT_EQ(9+2, cmds.stats.started.size());
        EXPECT_TRUE(cmds.stats.started.contains(janSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(pietSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.group.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanCmd.get()));

        // +2 for the optional outputs
        EXPECT_EQ(9+2, cmds.stats.selfExecuted.size());
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(janSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(pietSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.group.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanCmd.get()));

        EXPECT_EQ("piet", readFile(cmds.pietOut->absolutePath()));
        EXPECT_EQ("jan", readFile(cmds.janOut->absolutePath()));
        EXPECT_EQ("pietjan", readFile(cmds.pietjanOut->absolutePath()));
    }

    TEST(CommandNode, incrementalBuildWhileNoModifications) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        cmds.pietSrc->setState(Node::State::Dirty);
        cmds.janSrc->setState(Node::State::Dirty);

        EXPECT_EQ(Node::State::Dirty, cmds.pietCmd->state());
        EXPECT_EQ(Node::State::Dirty, cmds.janCmd->state());
        EXPECT_EQ(Node::State::Dirty, cmds.pietjanCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.janOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanOut->state());

        EXPECT_TRUE(cmds.execute());

        EXPECT_EQ(6, cmds.stats.started.size());
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.group.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanCmd.get()));

        EXPECT_EQ(3, cmds.stats.selfExecuted.size());
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.group.get()));

        // No last-write-times changed, hence no rehashes.
        EXPECT_EQ(0, cmds.stats.rehashedFiles.size());

        EXPECT_EQ(Node::State::Ok, cmds.pietCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.janCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.janOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanOut->state());
    }

    TEST(CommandNode, incrementalBuildAfterFileModification) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        std::ofstream janSrcFile(cmds.janSrc->absolutePath().string());
        janSrcFile << "janjan" << std::endl;
        janSrcFile.close();
        cmds.janSrc->setState(Node::State::Dirty);

        EXPECT_EQ(Node::State::Ok, cmds.pietCmd->state());
        EXPECT_EQ(Node::State::Dirty, cmds.janCmd->state());
        EXPECT_EQ(Node::State::Dirty, cmds.pietjanCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.janOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanOut->state());
        
        EXPECT_TRUE(cmds.execute());

        // +2 for the optional outputs
        EXPECT_EQ(5+2, cmds.stats.started.size());
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanOut.get()));

        // 1: pendingStartSelf of janCmd sees changed hash of janSrc
        // 2: self-execution of janCmd => updates and rehashes janOut 
        // 3: pendingStartSelf of pietjanCmd sees changed hash of janOut
        // 4: execution of pietjanCmd updates and rehashes pietjanOut
        // Note that janOut and pietjanOut are executed because Dirty
        // but that their time stamps have not changes and hence have
        // not added themselves to updateFiles.
        // +2 for the optional outputs
        EXPECT_EQ(5+2, cmds.stats.selfExecuted.size());
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanCmd.get()));
        EXPECT_EQ(3+2, cmds.stats.rehashedFiles.size()); // janSrc, janOut, pietjanOut, optional[12]

        EXPECT_EQ(Node::State::Ok, cmds.pietCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.janCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.janOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanOut->state());
    }

    TEST(CommandNode, incrementalBuildAfterFileDeletion) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());
        std::filesystem::remove(cmds.janOut->absolutePath());
        cmds.janOut->setState(Node::State::Dirty);

        EXPECT_EQ(Node::State::Ok, cmds.pietCmd->state());
        EXPECT_EQ(Node::State::Dirty, cmds.janCmd->state());
        EXPECT_EQ(Node::State::Dirty, cmds.pietjanCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
        EXPECT_EQ(Node::State::Dirty, cmds.janOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanOut->state());

        EXPECT_TRUE(cmds.execute());

        EXPECT_EQ(3, cmds.stats.started.size());
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janOut.get()));

        // 1: pendingStartSelf of janCmd sees changed hash of janOut
        // 2: self-execution of janCmd => restores janOut, no change in hash 
        // 3: pendingStartSelf of pietjanCmd sees unchanged hash of janOut,
        //    hence no re-execution
        EXPECT_EQ(2, cmds.stats.selfExecuted.size());
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janCmd.get()));
        EXPECT_EQ(1, cmds.stats.rehashedFiles.size());
        EXPECT_TRUE(cmds.stats.rehashedFiles.contains(cmds.janOut.get()));

        EXPECT_EQ(Node::State::Ok, cmds.pietCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.janCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.janOut->state());
        EXPECT_EQ(Node::State::Ok, cmds.pietjanOut->state());
    }

    TEST(CommandNode, fail_script) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        cmds.pietCmd->script("exit 1"); // execution fails

        EXPECT_TRUE(cmds.execute());
        ASSERT_EQ(Node::State::Failed, cmds.pietCmd->state());
        ASSERT_EQ(Node::State::Ok, cmds.janCmd->state());
        ASSERT_EQ(Node::State::Canceled, cmds.pietjanCmd->state());
        bool found = false;
        for (auto const& record : cmds.memLogBook->records()) {
            found = std::string::npos != record.message.find("Command script failed");
            if (found) break;
        }
        EXPECT_TRUE(found);
    }

    TEST(CommandNode, fail_inputFromMissingInputProducer) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        // pietjanCmd reads output files of pietCmd and janCmd.
        // Execution fails because janCmd is not in input producers
        // of pietjanCmd
        cmds.pietjanCmd->orderOnlyInputs({ cmds.pietOut });
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Failed, cmds.pietjanCmd->state());
        bool found = false;
        for (auto const& record : cmds.memLogBook->records()) {
            found = std::string::npos != record.message.find("Build order is not guaranteed");
            if (found) break;
        }
        EXPECT_TRUE(found);
    }

    TEST(CommandNode, fail_inputFromIndirectInputProducer) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        // pietjanCmd reads output files of pietCmd and janCmd.
        // Execution warns for indirect prerequisites because pietCmd is 
        // an indirect prerequisite(via janCmd) of pietjanCmd.
        cmds.janCmd->orderOnlyInputs({ cmds.pietOut });
        cmds.pietjanCmd->orderOnlyInputs({ cmds.janOut });
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Failed, cmds.pietjanCmd->state());
        bool found = false;
        for (auto const& record : cmds.memLogBook->records()) {
            found = std::string::npos != record.message.find("Build order is not guaranteed");
            if (found) break;
        }
        EXPECT_TRUE(found);}

    TEST(CommandNode, fail_outputToSourceFile) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        // Execution fails because pietCmd writes to source file.
        std::stringstream script;
        script << "echo piet > " << cmds.pietSrc->absolutePath().string();
        cmds.pietCmd->script(script.str());
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Failed, cmds.pietCmd->state());
        bool found = false;
        for (auto const& record : cmds.memLogBook->records()) {
            found = std::string::npos != record.message.find("Source file is updated by build");
            if (found) break;
        }
        EXPECT_TRUE(found);
    }

    TEST(CommandNode, fail_outputNotDeclared) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        // Execution fails because pietCmd writes to not declared 
        // output file
        cmds.pietCmd->mandatoryOutputs({ });
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Failed, cmds.pietCmd->state());
        bool found = false;
        for (auto const& record : cmds.memLogBook->records()) {
            found = std::string::npos != record.message.find("Not-declared output file");
            if (found) break;
        }
        EXPECT_TRUE(found);
    }

    TEST(CommandNode, fail_notExpectedOutputProducer) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        // Execution fails because pietCmd produces same output file as janCmd.
        std::stringstream script;
        script << "type " << cmds.pietSrc->absolutePath().string() << " > " << cmds.janOut->absolutePath().string();
        cmds.pietCmd->script(script.str());
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Failed, cmds.pietCmd->state());
        bool found = false;
        for (auto const& record : cmds.memLogBook->records()) {
            found = std::string::npos != record.message.find("Not-declared output file is produced by 2 commands");
            if (found) break;
        }
        EXPECT_TRUE(found);
    }
}

