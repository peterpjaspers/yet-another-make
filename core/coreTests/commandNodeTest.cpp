#include "executeNode.h"
#include "../CommandNode.h"
#include "../SourceFileNode.h"
#include "../GeneratedFileNode.h"
#include "../ThreadPool.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../MultiwayLogBook.h"
#include "../MemoryLogBook.h"
#include "../BasicOStreamLogBook.h"
#include "../FileRepository.h"

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

    std::filesystem::path np(std::filesystem::path const& path) {
        return FileSystem::normalizePath(path);
    }

    class Commands
    {
    public:
        std::filesystem::path repoDir;
        std::shared_ptr<MemoryLogBook> memLogBook;
        std::shared_ptr<BasicOStreamLogBook> stdoutLogBook;
        std::shared_ptr<MultiwayLogBook> logBook;
        ExecutionContext context;
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
            , pietCmd(std::make_shared<CommandNode>(&context, np("piet\\_cmd")))
            , janCmd(std::make_shared<CommandNode>(& context, np("jan\\_cmd")))
            , pietjanCmd(std::make_shared<CommandNode>(& context, np("pietjan\\_cmd")))
            , pietOut(std::make_shared<GeneratedFileNode>(& context, np(repoDir / "generated\\pietOut.txt"), pietCmd.get()))
            , janOut(std::make_shared<GeneratedFileNode>(&context, np(repoDir / "generated\\janOut.txt"), janCmd.get()))
            , pietjanOut(std::make_shared<GeneratedFileNode>(&context, np(repoDir / "generated\\pietjanOut.txt"), pietjanCmd.get()))
            , pietSrc(std::make_shared<SourceFileNode>(&context, np(repoDir / "pietSrc.txt")))
            , janSrc(std::make_shared<SourceFileNode>(&context, np(repoDir / "janSrc.txt")))
            , stats(context.statistics())
        {
            std::filesystem::create_directories(repoDir);
            std::filesystem::create_directories(np(repoDir / "generated"));
            logBook->add(memLogBook);
            logBook->add(stdoutLogBook);
            context.logBook(logBook);
            context.addRepository(
                std::make_shared<FileRepository>(
                    "windows",
                    std::filesystem::path("c:\\windows")));
            context.addRepository(
                std::make_shared<FileRepository>(
                    "repo",
                    repoDir));
            stats.registerNodes = true;
            //context.threadPool().size(1); // to ease debugging

            std::ofstream pietSrcFile(pietSrc->name().string());
            pietSrcFile << "piet";
            pietSrcFile.close();
            std::ofstream janSrcFile(janSrc->name().string());
            janSrcFile << "jan";
            janSrcFile.close();

            {
                std::stringstream script;
                script << "type " << pietSrc->name().string() << " > " << pietOut->name().string();
                pietCmd->outputs({ pietOut });
                pietCmd->script(script.str());
            }
            {
                std::stringstream script;
                script << "type " << janSrc->name().string() << " > " << janOut->name().string();
                janCmd->outputs({ janOut });
                janCmd->script(script.str());
            }
            {
                std::stringstream script;
                pietjanCmd->outputs({ pietjanOut });
                pietjanCmd->inputProducers({ pietCmd, janCmd });
                script
                    << "type " << pietOut->name().string() << " > " << pietjanOut->name().string()
                    << " & type " << janOut->name().string() << " >> " << pietjanOut->name().string();
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

        EXPECT_EQ(8, cmds.stats.started.size());
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanCmd.get()));

        EXPECT_EQ(8, cmds.stats.selfExecuted.size());
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanCmd.get()));
            
        EXPECT_EQ("piet", readFile(cmds.pietOut->name()));
        EXPECT_EQ("jan", readFile(cmds.janOut->name()));
        EXPECT_EQ("pietjan", readFile(cmds.pietjanOut->name()));
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

        EXPECT_EQ(8, cmds.stats.started.size());
        EXPECT_TRUE(cmds.stats.started.contains(janSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(pietSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanOut.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanCmd.get()));

        EXPECT_EQ(8, cmds.stats.selfExecuted.size());
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(janSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(pietSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanCmd.get()));

        EXPECT_EQ("piet", readFile(cmds.pietOut->name()));
        EXPECT_EQ("jan", readFile(cmds.janOut->name()));
        EXPECT_EQ("pietjan", readFile(cmds.pietjanOut->name()));
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

        EXPECT_EQ(5, cmds.stats.started.size());
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janSrc.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietCmd.get()));
        EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanCmd.get()));

        EXPECT_EQ(2, cmds.stats.selfExecuted.size());
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janSrc.get()));

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

        std::ofstream janSrcFile(cmds.janSrc->name().string());
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

        EXPECT_EQ(5, cmds.stats.started.size());
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
        EXPECT_EQ(5, cmds.stats.selfExecuted.size());
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janSrc.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janCmd.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanOut.get()));
        EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanCmd.get()));
        EXPECT_EQ(3, cmds.stats.rehashedFiles.size()); // janSrc, janOut, pietjanOut

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
        std::filesystem::remove(cmds.janOut->name());
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
        ASSERT_NE(std::string::npos, cmds.memLogBook->records()[0].message.find("Command script failed"));
    }

    TEST(CommandNode, fail_inputFromMissingInputProducer) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        // pietjanCmd reads output files of pietCmd and janCmd.
        // Execution fails because janCmd is not in input producers
        // of pietjanCmd
        cmds.pietjanCmd->inputProducers({ cmds.pietCmd });
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Failed, cmds.pietjanCmd->state());
        EXPECT_NE(std::string::npos, cmds.memLogBook->records()[0].message.find("Build order is not guaranteed"));
    }

    TEST(CommandNode, fail_inputFromIndirectInputProducer) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        // pietjanCmd reads output files of pietCmd and janCmd.
        // Execution warns for indirect prerequisites because pietCmd is 
        // an indirect prerequisite(via janCmd) of pietjanCmd.
        cmds.janCmd->inputProducers({ cmds.pietCmd });
        cmds.pietjanCmd->inputProducers({ cmds.janCmd });
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Failed, cmds.pietjanCmd->state());
        EXPECT_NE(std::string::npos, cmds.memLogBook->records()[0].message.find("Build order is not guaranteed"));
    }

    TEST(CommandNode, fail_outputToSourceFile) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        // Execution fails because pietCmd writes to source file.
        std::stringstream script;
        script << "echo piet > " << cmds.pietSrc->name().string();
        cmds.pietCmd->script(script.str());
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Failed, cmds.pietCmd->state());
        EXPECT_NE(std::string::npos, cmds.memLogBook->records()[0].message.find("Source file is updated by build"));
    }

    TEST(CommandNode, fail_outputNotDeclared) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        // Execution fails because pietjanCmd writes to not declared 
        // output file
        cmds.pietCmd->outputs({ });
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Failed, cmds.pietCmd->state());
        EXPECT_NE(std::string::npos, cmds.memLogBook->records()[0].message.find("Mismatch between declared outputs and actual outputs"));
    }

    TEST(CommandNode, warning_inputNotInARepository) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        std::ofstream janSrcFile(cmds.janSrc->name().string());
        janSrcFile << "janjan" << std::endl;
        janSrcFile.close();
        cmds.janSrc->setState(Node::State::Dirty);
        cmds.context.removeRepository("repo");
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Ok, cmds.janCmd->state());
        EXPECT_NE(std::string::npos, cmds.memLogBook->records()[0].message.find("Input file ignored because not in a known file repository"));
    }

    TEST(CommandNode, fail_notExpectedOutputProducer) {
        Commands cmds;

        EXPECT_TRUE(cmds.execute());

        // Execution fails because pietCmd produces same output file as janCmd.
        std::stringstream script;
        script << "type " << cmds.pietSrc->name().string() << " > " << cmds.janOut->name().string();
        cmds.pietCmd->script(script.str());
        EXPECT_TRUE(cmds.execute());
        EXPECT_EQ(Node::State::Failed, cmds.pietCmd->state());
        EXPECT_NE(std::string::npos, cmds.memLogBook->records()[0].message.find("Output file is produced by 2 commands"));
    }

}

