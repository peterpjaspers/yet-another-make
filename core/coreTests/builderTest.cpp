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
		std::filesystem::path startDir;
		std::filesystem::path pietH;
		std::filesystem::path janH;
		std::filesystem::path pietCpp;
		std::filesystem::path janCpp;

		// Create unique directory dir and sub directories dir/src 
		// and dir/generated.
		// Fill repoDir/src with C++ source files.
		// Change current directory to dir. Return to initial current
		// directory on destruction to unlock dir for deletion.
		TestRepository()
			: dir(FileSystem::createUniqueDirectory())
			, startDir(std::filesystem::current_path())
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
			janCppFile << "int jan(int x) { return x + 5; }";
			janCppFile.close();
		}

		~TestRepository() {
			std::filesystem::current_path(startDir);
			std::filesystem::remove_all(dir);
		}
	};

	std::shared_ptr<BuildResult> initializeYam(std::filesystem::path const& dir) {
		auto request = std::make_shared< BuildRequest>(BuildRequest::RequestType::Init);
		request->directory(dir);
		Dispatcher dispatcher;
		DispatcherFrame frame;
		std::shared_ptr<BuildResult> result;
		Builder builder;
		builder.completor().AddLambda(
			[&result, &frame](std::shared_ptr<BuildResult> r) {
				result = r;
		        frame.stop();
			});
		builder.start(request);
		dispatcher.run(&frame);
		return result;
	}

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
			, pietOut(std::make_shared<GeneratedFileNode>(context, repo.dir / "generated" / "pietout.obj", ccPiet))
			, janOut(std::make_shared<GeneratedFileNode>(context, repo.dir / "generated"/ "janout.obj", ccJan))
			, pietjanOut(std::make_shared<GeneratedFileNode>(context, repo.dir / "generated" / "pietjanout.dll", linkPietJan))
			, stats(context->statistics())
		{
			stats.registerNodes = true;
			// context->threadPool().size(1); // to ease debugging

			// TODO: create compile and link scripts
			{
				std::stringstream script;
				script 
					<< "type " << repo.pietCpp.string() << " > " << pietOut->name().string()
					<< " & type " << repo.pietH.string() << " >> " << pietOut->name().string() << std::endl;
				ccPiet->setOutputs({ pietOut });
				ccPiet->setScript(script.str());
			}
			{
				std::stringstream script;
				script 
					<< "type " << repo.janCpp.string() << " > " << janOut->name().string()
					<< " & type " << repo.janH.string() << " >> " << janOut->name().string() << std::endl;

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

	TEST(Builder, build) {
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

		std::vector<std::shared_ptr<Node>> inputs;
		driver.ccPiet->getInputs(inputs);
		EXPECT_EQ(2, inputs.size());
		auto pietCpp = driver.context->nodes().find(driver.repo.pietCpp);
		auto pietH = driver.context->nodes().find(driver.repo.pietH);
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), pietCpp));
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), pietH));

		inputs.clear();
		driver.ccJan->getInputs(inputs);
		EXPECT_EQ(2, inputs.size());
		auto janCpp = driver.context->nodes().find(driver.repo.janCpp);
		auto janH = driver.context->nodes().find(driver.repo.janH);
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), janCpp));
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), janH));;

		inputs.clear();
		driver.linkPietJan->getInputs(inputs);
		EXPECT_EQ(2, inputs.size());
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), driver.pietOut));
		EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), driver.janOut));
	}
}

