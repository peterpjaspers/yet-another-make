#include "../Builder.h"
#include "../CommandNode.h"
#include "../SourceFileNode.h"
#include "../GeneratedFileNode.h"
#include "../SourceFileRepository.h"
#include "../ThreadPool.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../RegexSet.h"
#include "../BuildRequest.h"
#include "../BuildResult.h"
#include "../Dispatcher.h"
#include "../DispatcherFrame.h"

#include "gtest/gtest.h"
#include <stdlib.h>
#include <memory.h>
#include <fstream>

namespace
{
	using namespace YAM;

	std::string readFile(std::filesystem::path path) {
		std::ifstream file(path.string());
		std::string line, content;
		while (getline(file, line)) content += line;
		file.close();
		return content;
	}

	RegexSet excludes({
				RegexSet::matchDirectory("generated"),
				RegexSet::matchDirectory(".yam"),
		});

	class TestRepository
	{
	public:
		std::filesystem::path dir;
		std::filesystem::path pietH;
		std::filesystem::path janH;
		std::filesystem::path pietCpp;
		std::filesystem::path janCpp;
		std::string pietHContent;
		std::string pietCppContent;
		std::string janHContent;
		std::string janCppContent;

		// Create unique directory dir and sub directories dir/src 
		// and dir/generated.
		// Fill repoDir/src with C++ source files.
		TestRepository()
			: dir(FileSystem::createUniqueDirectory())
			, pietH(dir / "src" / "piet.h")
			, janH(dir / "src" / "jan.h")
			, pietCpp(dir / "src" / "piet.cpp")
			, janCpp(dir / "src" / "jan.cpp")
		{
			std::filesystem::create_directories(dir / "src");
			std::filesystem::create_directories(dir / "generated");

			std::ofstream pietHFile(pietH.string());
			pietHFile << "int piet(int x);";
			pietHFile.close();
			std::ofstream janHFile(janH.string());
			janHFile << "int jan(int x);";
			janHFile.close();

			std::ofstream pietCppFile(pietCpp.string());
			pietCppFile
				<< R"(#include "piet.h")" << std::endl
				<< R"(#include "jan.h")" << std::endl
				<< R"(int piet(int x) { return jan(x) + 3; })" << std::endl;
			pietCppFile.close();
			std::ofstream janCppFile(janCpp.string());
			janCppFile
				<< R"(#include "jan.h")" << std::endl
				<< R"(int jan(int x) { return x + 5; })" << std::endl; 
			janCppFile.close();

			pietHContent = readFile(pietH);
			pietCppContent = readFile(pietCpp);
			janCppContent = readFile(janCpp);
			janHContent = readFile(janH);
		}

		~TestRepository() {
			std::filesystem::remove_all(dir);
		}
	};

	class TestDriver
	{
	public:
		TestRepository repo;
		Builder builder;
		ExecutionContext* context;
		std::shared_ptr<CommandNode> ccPiet;
		std::shared_ptr<CommandNode> ccJan;
		std::shared_ptr<CommandNode> linkPietJan;
		std::shared_ptr<GeneratedFileNode> pietOut;
		std::shared_ptr<GeneratedFileNode> janOut;
		std::shared_ptr<GeneratedFileNode> pietjanOut;
		ExecutionStatistics& stats;

		// Create repo and builder,fill builder context with CommandNodes that 
		// compile the source files in repo and that link the resulting object 
		// files into a dll.
		TestDriver()
			: repo()
			, builder()
			, context(builder.context())
			, ccPiet(std::make_shared<CommandNode>(context, repo.dir / "ccpiet"))
			, ccJan(std::make_shared<CommandNode>(context, repo.dir / "ccjan"))
			, linkPietJan(std::make_shared<CommandNode>(context, repo.dir / "linkpietjan"))
			, pietOut(std::make_shared<GeneratedFileNode>(context, repo.dir / "generated" / "pietout.obj", ccPiet.get()))
			, janOut(std::make_shared<GeneratedFileNode>(context, repo.dir / "generated"/ "janout.obj", ccJan.get()))
			, pietjanOut(std::make_shared<GeneratedFileNode>(context, repo.dir / "generated" / "pietjanout.dll", linkPietJan.get()))
			, stats(context->statistics())
		{
			stats.registerNodes = true;
			// context->threadPool().size(1); // to ease debugging

			// Simulate compilation and linking
			{
				std::stringstream script;
				script
					<< "type " << repo.pietH.string() << " > " << pietOut->name().string()
					<< " & type " << repo.janH.string() << " >> " << pietOut->name().string()
					<< " & type " << repo.pietCpp.string() << " >> " << pietOut->name().string() << std::endl;
				ccPiet->setOutputs({ pietOut });
				ccPiet->setScript(script.str());
			}
			{
				std::stringstream script;
				script 
					<< "type " << repo.janH.string() << " > " << janOut->name().string()
					<< " & type " << repo.janCpp.string() << " >> " << janOut->name().string() << std::endl;

				ccJan->setOutputs({ janOut });
				ccJan->setScript(script.str());
			}
			{
				std::stringstream script;
				linkPietJan->setOutputs({ pietjanOut });
				linkPietJan->setInputProducers({ ccPiet, ccJan });
				script
					<< "type " << pietOut->name().string() << " > " << pietjanOut->name().string()
					<< " & type " << janOut->name().string() << " >> " << pietjanOut->name().string() << std::endl;
				linkPietJan->setScript(script.str());
			}

			context->nodes().add(ccPiet);
			context->nodes().add(pietOut);
			context->nodes().add(ccJan);
			context->nodes().add(janOut);
			context->nodes().add(linkPietJan);
			context->nodes().add(pietjanOut);

			EXPECT_EQ(Node::State::Dirty, ccPiet->state());
			EXPECT_EQ(Node::State::Dirty, ccJan->state());
			EXPECT_EQ(Node::State::Dirty, linkPietJan->state());
			EXPECT_EQ(Node::State::Dirty, pietOut->state());
			EXPECT_EQ(Node::State::Dirty, janOut->state());
			EXPECT_EQ(Node::State::Dirty, pietjanOut->state());
		}

		std::shared_ptr<SourceFileRepository> sourceRepo() {
			return context->findRepository(repo.dir.filename());
		}

		Node* findNode(std::filesystem::path const& path) {
			auto sptr = context->nodes().find(path);
			return sptr == nullptr ? nullptr : sptr.get();
		}

		std::shared_ptr<BuildResult> executeRequest(std::shared_ptr<BuildRequest> request) {
			Dispatcher dispatcher;
			std::atomic<std::shared_ptr<BuildResult>> result;
			builder.completor().AddLambda(
				[&result, &dispatcher](std::shared_ptr<BuildResult> r) {
					result = r;
			        dispatcher.stop();
				});
			builder.start(request);
			dispatcher.run();
			builder.completor().RemoveAll();
			return result;
		}

		std::shared_ptr<BuildResult> initializeYam() {
			auto request = std::make_shared< BuildRequest>(BuildRequest::RequestType::Init);
			request->directory(repo.dir);
			return executeRequest(request);
		}

		std::shared_ptr<BuildResult> build() {
			auto request = std::make_shared< BuildRequest>(BuildRequest::RequestType::Build);
			request->directory(repo.dir);
			return executeRequest(request);
		}

		std::string expectedPietOutContent() { return repo.pietHContent + repo.janHContent + repo.pietCppContent; }
		std::string expectedJanOutContent() { return repo.janHContent + repo.janCppContent; }
		std::string expectedPietjanOutContent() { return expectedPietOutContent() + expectedJanOutContent();}
		std::string actualPietOutContent() { return readFile(pietOut->name()); }
		std::string actualJanOutContent() { return readFile(janOut->name()); }
		std::string actualPietjanOutContent() { return readFile(pietjanOut->name()); }
	};

	TEST(Builder, initOnce) {
		TestDriver driver;

		std::shared_ptr<BuildResult> result = driver.initializeYam();
		EXPECT_TRUE(result->succeeded());
		auto srcRepo = driver.builder.context()->findRepository(driver.repo.dir.filename().string());
		EXPECT_NE(nullptr, srcRepo);
		EXPECT_EQ(driver.repo.dir, srcRepo->directoryName());
	}
	TEST(Builder, initTwice) {
		TestDriver driver;

		driver.initializeYam();
		std::shared_ptr<BuildResult> result = driver.initializeYam();
		EXPECT_FALSE(result->succeeded());
	}

	TEST(Builder, firstBuild) {
		TestDriver driver;

		driver.initializeYam();
		std::shared_ptr<BuildResult> result = driver.build();

		EXPECT_TRUE(result->succeeded());
		EXPECT_EQ(Node::State::Ok, driver.ccPiet->state());
		EXPECT_EQ(Node::State::Ok, driver.pietOut->state());
		EXPECT_EQ(Node::State::Ok, driver.ccJan->state());
		EXPECT_EQ(Node::State::Ok, driver.janOut->state());
		EXPECT_EQ(Node::State::Ok, driver.linkPietJan->state());
		EXPECT_EQ(Node::State::Ok, driver.pietjanOut->state());

		EXPECT_EQ(driver.expectedPietOutContent(), driver.actualPietOutContent());
		EXPECT_EQ(driver.expectedJanOutContent(), driver.actualJanOutContent());
		EXPECT_EQ(driver.expectedPietjanOutContent(), driver.actualPietjanOutContent());
		
		// Verify input files of command nodes
		std::vector<std::shared_ptr<Node>> inputs;
		driver.ccPiet->getInputs(inputs);
		EXPECT_EQ(3, inputs.size());
		auto pietCpp = driver.context->nodes().find(driver.repo.pietCpp);
		auto pietH = driver.context->nodes().find(driver.repo.pietH);
		auto janH = driver.context->nodes().find(driver.repo.janH);
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), pietCpp));
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), pietH));
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), janH));

		inputs.clear();
		driver.ccJan->getInputs(inputs);
		EXPECT_EQ(2, inputs.size());
		auto janCpp = driver.context->nodes().find(driver.repo.janCpp);
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), janCpp));
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), janH));;

		inputs.clear();
		driver.linkPietJan->getInputs(inputs);
		EXPECT_EQ(2, inputs.size());
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), driver.pietOut));
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), driver.janOut));

		// Verify nodes started for execution 
		// 1  command node __scope (from Builder) for executing directorynode
		// 2  command node __scope for executing compile and link nodes
		// 3-4 dir nodes repo.dir, repo.dir/src
		// 5-6 source file nodes repo.dir/src/pietCpp, repo.dir/src/pietH
		// 7-8 source file nodes repo.dir/src/janCpp, repo.dir/src/janH
		// 9-11 generated file nodes pietOut, janOut, pietjanOut
		// 12-14 commandNodes ccPiet.cpp, ccJan linkPietjan
		EXPECT_EQ(14, driver.stats.nStarted); // __scope is started twice..
		EXPECT_EQ(13, driver.stats.started.size()); // ..started contains unique nodes
		EXPECT_EQ(2, driver.stats.nDirectoryUpdates);
		EXPECT_EQ(7, driver.stats.nFileUpdates); // 4 source + 3 generated

		// Note: __scope has been already been destroyed by Builder
		EXPECT_TRUE(driver.stats.started.contains(driver.findNode(driver.repo.dir)));
		EXPECT_TRUE(driver.stats.started.contains(driver.findNode(driver.repo.dir / "src")));
		EXPECT_TRUE(driver.stats.started.contains(driver.findNode(driver.repo.pietCpp)));
		EXPECT_TRUE(driver.stats.started.contains(driver.findNode(driver.repo.pietH)));
		EXPECT_TRUE(driver.stats.started.contains(driver.findNode(driver.repo.janCpp)));
		EXPECT_TRUE(driver.stats.started.contains(driver.findNode(driver.repo.janH)));
		EXPECT_TRUE(driver.stats.started.contains(driver.ccPiet.get()));
		EXPECT_TRUE(driver.stats.started.contains(driver.ccJan.get()));
		EXPECT_TRUE(driver.stats.started.contains(driver.linkPietJan.get()));
		EXPECT_TRUE(driver.stats.started.contains(driver.pietOut.get()));
		EXPECT_TRUE(driver.stats.started.contains(driver.janOut.get()));
		EXPECT_TRUE(driver.stats.started.contains(driver.pietjanOut.get()));

		// Verify nodes self-executed 
		// Note: __scope node is executed twice (repo directory and compile and link commands)
		EXPECT_EQ(14, driver.stats.nSelfExecuted); // __scope self-executed twice..
		EXPECT_EQ(13, driver.stats.selfExecuted.size()); // ..selfExectued contains unique nodes.
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.findNode(driver.repo.dir)));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.findNode(driver.repo.dir / "src")));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.findNode(driver.repo.pietCpp)));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.findNode(driver.repo.pietH)));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.findNode(driver.repo.janCpp)));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.findNode(driver.repo.janH)));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.ccPiet.get()));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.ccJan.get()));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.linkPietJan.get()));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.pietOut.get()));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.janOut.get()));
		EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.pietjanOut.get()));
	}


	TEST(Builder, incrementalBuildWhileNoModifications) {
		TestDriver driver;

		// First build
		driver.initializeYam();
		driver.build();
		driver.stats.reset();

		// Incremental build while no modifications
		std::shared_ptr<BuildResult> result = driver.build();
		EXPECT_TRUE(result->succeeded());
		EXPECT_EQ(0, driver.stats.nDirectoryUpdates);
		EXPECT_EQ(0, driver.stats.nFileUpdates);
		EXPECT_EQ(0, driver.stats.started.size());
		EXPECT_EQ(0, driver.stats.selfExecuted.size());
	}
	
/*
	TEST(Builder, incrementalBuildAfterFileModification) {
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
		EXPECT_EQ(Node::State::Dirty, cmds.janOut->state());
		EXPECT_EQ(Node::State::Dirty, cmds.pietjanOut->state());

		EXPECT_TRUE(cmds.execute());

		EXPECT_EQ(4, cmds.stats.started.size());
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
		EXPECT_EQ(4, cmds.stats.selfExecuted.size());
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janOut.get()));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janCmd.get()));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanOut.get()));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanCmd.get()));
		EXPECT_EQ(0, cmds.stats.rehashedFiles.size());

		EXPECT_EQ(Node::State::Ok, cmds.pietCmd->state());
		EXPECT_EQ(Node::State::Ok, cmds.janCmd->state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd->state());
		EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
		EXPECT_EQ(Node::State::Ok, cmds.janOut->state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjanOut->state());
	}

	TEST(Builder, incrementalBuildAfterFileDeletion) {
		Commands cmds;

		EXPECT_TRUE(cmds.execute());
		std::filesystem::remove(cmds.janOut->name());
		cmds.janOut->setState(Node::State::Dirty);

		EXPECT_EQ(Node::State::Ok, cmds.pietCmd->state());
		EXPECT_EQ(Node::State::Dirty, cmds.janCmd->state());
		EXPECT_EQ(Node::State::Dirty, cmds.pietjanCmd->state());
		EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
		EXPECT_EQ(Node::State::Dirty, cmds.janOut->state());
		EXPECT_EQ(Node::State::Dirty, cmds.pietjanOut->state());

		EXPECT_TRUE(cmds.execute());

		EXPECT_EQ(4, cmds.stats.started.size());
		EXPECT_TRUE(cmds.stats.started.contains(cmds.janCmd.get()));
		EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanCmd.get()));
		EXPECT_TRUE(cmds.stats.started.contains(cmds.janOut.get()));
		EXPECT_TRUE(cmds.stats.started.contains(cmds.pietjanOut.get()));

		// 1: pendingStartSelf of janCmd sees changed hash of janOut
		// 2: self-execution of janCmd => restores janOut, no change in hash 
		// 3: pendingStartSelf of pietjanCmd sees unchanged hash of janOut,
		//    hence no re-execution
		// Note that janOut and pietjanOut are executed because Dirty
		// but that their time stamps have not changes and hence have
		// not added themselves to updateFiles.
		EXPECT_EQ(3, cmds.stats.selfExecuted.size());
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janOut.get()));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.janCmd.get()));
		EXPECT_TRUE(cmds.stats.selfExecuted.contains(cmds.pietjanOut.get()));
		EXPECT_EQ(1, cmds.stats.rehashedFiles.size());
		EXPECT_TRUE(cmds.stats.rehashedFiles.contains(cmds.janOut.get()));

		EXPECT_EQ(Node::State::Ok, cmds.pietCmd->state());
		EXPECT_EQ(Node::State::Ok, cmds.janCmd->state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjanCmd->state());
		EXPECT_EQ(Node::State::Ok, cmds.pietOut->state());
		EXPECT_EQ(Node::State::Ok, cmds.janOut->state());
		EXPECT_EQ(Node::State::Ok, cmds.pietjanOut->state());
	}
*/

}

