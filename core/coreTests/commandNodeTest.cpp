#include "executeNode.h"
#include "../CommandNode.h"
#include "../SourceFileNode.h"
#include "../GeneratedFileNode.h"
#include "../ThreadPool.h"
#include "../ExecutionContext.h"
#include "../../xxhash/xxhash.h"

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
		ExecutionContext context;
		CommandNode pietCmd;
		CommandNode janCmd;
		CommandNode pietjanCmd;
		GeneratedFileNode piet;
		GeneratedFileNode jan;
		GeneratedFileNode pietjan;
		ExecutionStatistics& stats;

		Commands()
			: pietCmd(&context, "piet/_cmd")
			, janCmd(&context, "jan/_cmd")
			, pietjanCmd(&context, "pietjan/_cmd")
			, piet(&context, "piet.txt", &pietCmd)
			, jan(&context, "jan.txt", &janCmd)
			, pietjan(&context, "pietjan.txt", &pietjanCmd)
			, stats(context.statistics())
		{
			auto shell = boost::process::search_path("cmd").string();
			clean();
			stats.reset();
			stats.registerNodes = true;

			pietCmd.setOutputs({ &piet });
			pietCmd.setScript(shell + " /c echo piet > piet.txt");

			janCmd.setOutputs({ &jan });
			janCmd.setScript(shell + " /c echo jan > jan.txt");

			pietjanCmd.setOutputs({ &pietjan });
			pietjanCmd.setInputProducers({ &pietCmd, &janCmd });
			pietjanCmd.setScript(shell + " /c type piet.txt > pietjan.txt & type jan.txt >> pietjan.txt");

			EXPECT_EQ(Node::State::Dirty, pietCmd.state());
			EXPECT_EQ(Node::State::Dirty, janCmd.state());
			EXPECT_EQ(Node::State::Dirty, pietjanCmd.state());
			EXPECT_EQ(Node::State::Dirty, piet.state());
			EXPECT_EQ(Node::State::Dirty, jan.state());
			EXPECT_EQ(Node::State::Dirty, pietjan.state());

			// convenient for debugging: limit threadpool to 1 thread
			//
			//context.threadPool().size(1);
		}

		~Commands() {
			clean();
		}

		void clean() {
			std::filesystem::remove("piet.txt");
			std::filesystem::remove("jan.txt");
			std::filesystem::remove("pietjan.txt");
		}

		bool execute() {
			return executeNode(&pietjanCmd);
		}
	};

	TEST(CommandNode, cleanBuild) {
		Commands cmds;

		EXPECT_TRUE(cmds.execute());
		EXPECT_EQ(Node::State::Ok, cmds.pietCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.janCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.piet.state());
		EXPECT_EQ(Node::State::Ok, cmds.jan.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjan.state());

		EXPECT_EQ(6, cmds.stats.started.size());
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.piet)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.jan)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.pietjan)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.janCmd)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.pietCmd)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.pietjanCmd)));

		EXPECT_EQ(6, cmds.stats.selfExecuted.size());
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.piet)));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.jan)));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.pietjan)));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.janCmd)));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.pietCmd)));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.pietjanCmd)));
			
		EXPECT_EQ("piet ", readFile("piet.txt"));
		EXPECT_EQ("jan ", readFile("jan.txt"));
		EXPECT_EQ("piet jan ", readFile("pietjan.txt"));
	}

	TEST(CommandNode, incrementalBuildWhileNoModifications) {
		Commands cmds;

		EXPECT_TRUE(cmds.execute());

		cmds.piet.setState(Node::State::Dirty);
		cmds.jan.setState(Node::State::Dirty);
		EXPECT_EQ(Node::State::Dirty, cmds.pietCmd.state());
		EXPECT_EQ(Node::State::Dirty, cmds.janCmd.state());
		EXPECT_EQ(Node::State::Dirty, cmds.pietjanCmd.state());
		EXPECT_EQ(Node::State::Dirty, cmds.piet.state());
		EXPECT_EQ(Node::State::Dirty, cmds.jan.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjan.state());

		cmds.stats.reset();
		EXPECT_TRUE(cmds.execute());

		EXPECT_EQ(5, cmds.stats.started.size());
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.piet)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.jan)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.janCmd)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.pietCmd)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.pietjanCmd)));

		EXPECT_EQ(2, cmds.stats.selfExecuted.size());
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.piet)));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.jan)));

		EXPECT_EQ(Node::State::Ok, cmds.pietCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.janCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.piet.state());
		EXPECT_EQ(Node::State::Ok, cmds.jan.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjan.state());
	}

	TEST(CommandNode, incrementalBuildAfterFileModification) {
		Commands cmds;

		EXPECT_TRUE(cmds.execute());

		std::ofstream myfile;
		myfile.open("jan.txt");
		myfile << "Modified this file.\n";
		myfile.close();
		cmds.jan.setState(Node::State::Dirty);

		EXPECT_EQ(Node::State::Ok, cmds.pietCmd.state());
		EXPECT_EQ(Node::State::Dirty, cmds.janCmd.state());
		EXPECT_EQ(Node::State::Dirty, cmds.pietjanCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.piet.state());
		EXPECT_EQ(Node::State::Dirty, cmds.jan.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjan.state());

		cmds.stats.reset();
		EXPECT_TRUE(cmds.execute());

		EXPECT_EQ(3, cmds.stats.started.size());
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.jan)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.janCmd)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.pietjanCmd)));
		
		// 1: self-execution of jan => from hash("jan") to hash("Modified this file.\n") 
		// 2: pendingStartSelf of janCmd sees changed hash of jan => execute janCmd
		// 3: execution of janCmd restores jan.txt to "jan", rehashes jan, recomputes
		//    execution hash of janCmd. Note that previous and new execution hashes are
		//    both computed from jan 'seeing' content "jan" in jan.txt, hence execution 
		//    hash of janCmd does not change.
		// 4: pendingStartSelf of pietjanCmd sees no changes in the hashes of its prerequisites,
		//    hence no self-execution of pietjanCmd
		EXPECT_EQ(2, cmds.stats.selfExecuted.size());
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.jan)));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.janCmd)));

		EXPECT_EQ(Node::State::Ok, cmds.pietCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.janCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.piet.state());
		EXPECT_EQ(Node::State::Ok, cmds.jan.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjan.state());
	}

	TEST(CommandNode, incrementalBuildAfterFileDeletion) {
		Commands cmds;

		EXPECT_TRUE(cmds.execute());
		std::filesystem::remove("jan.txt");
		cmds.jan.setState(Node::State::Dirty);

		EXPECT_EQ(Node::State::Ok, cmds.pietCmd.state());
		EXPECT_EQ(Node::State::Dirty, cmds.janCmd.state());
		EXPECT_EQ(Node::State::Dirty, cmds.pietjanCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.piet.state());
		EXPECT_EQ(Node::State::Dirty, cmds.jan.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjan.state());

		cmds.stats.reset();
		EXPECT_TRUE(cmds.execute());

		EXPECT_EQ(3, cmds.stats.started.size());
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.jan)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.janCmd)));
		EXPECT_TRUE(cmds.stats.started.contains(&(cmds.pietjanCmd)));

		// Note: execution of janCmd restores jan => execution hash of janCmd 
		// does not change => pietjanCmd not pendingSelfStart

		// 1: self-execution of jan => from hash("jan") to hash of deleted file) 
		// 2: pendingStartSelf of janCmd sees changed hash of jan => execute janCmd
		// 3: execution of janCmd restores jan.txt to "jan", rehashes jan, recomputes
		//    execution hash of janCmd. Note that previous and new execution hashes are
		//    both computed from jan 'seeing' content "jan" in jan.txt, hence execution 
		//    hash of janCmd does not change.
		// 4: pendingStartSelf of pietjanCmd sees no changes in the hashes of its prerequisites,
		//    hence no self-execution of pietjanCmd
		EXPECT_EQ(2, cmds.stats.selfExecuted.size());
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.jan)));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(&(cmds.janCmd)));

		EXPECT_EQ(Node::State::Ok, cmds.pietCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.janCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd.state());
		EXPECT_EQ(Node::State::Ok, cmds.piet.state());
		EXPECT_EQ(Node::State::Ok, cmds.jan.state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjan.state());
	}
}

