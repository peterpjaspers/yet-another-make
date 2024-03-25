#include "../Builder.h"
#include "../CommandNode.h"
#include "../SourceFileNode.h"
#include "../GeneratedFileNode.h"
#include "../DirectoryNode.h"
#include "../FileRepositoryNode.h"
#include "../ThreadPool.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../RegexSet.h"
#include "../BuildRequest.h"
#include "../BuildResult.h"
#include "../Dispatcher.h"
#include "../DispatcherFrame.h"
#include "../DotYamDirectory.h"
#include "../RepositoryNameFile.h"

#include "gtest/gtest.h"
#include <stdlib.h>
#include <memory.h>
#include <fstream>
#include <boost/process.hpp>

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
        std::string repoName;
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
        TestDriver(bool initialize = true)
            : repo()
            , repoName("testRepo")
            , context(builder.context())
            , ccPiet(std::make_shared<CommandNode>(context, R"(@@testRepo\ccpiet)"))
            , ccJan(std::make_shared<CommandNode>(context, R"(@@testRepo\ccjan)"))
            , linkPietJan(std::make_shared<CommandNode>(context, R"(@@testRepo\linkpietjan)"))
            , pietOut(std::make_shared<GeneratedFileNode>(context, R"(@@testRepo\generated\pietout.obj)", ccPiet))
            , janOut(std::make_shared<GeneratedFileNode>(context, R"(@@testRepo\generated\janout.obj)", ccJan))
            , pietjanOut(std::make_shared<GeneratedFileNode>(context, R"(@@testRepo\generated\pietjanout.dll)", linkPietJan))
            , stats(context->statistics())
        {
            stats.registerNodes = true;
            if (initialize) {
                DotYamDirectory::initialize(repo.dir, context->logBook().get());
                RepositoryNameFile nameFile(repo.dir);
                nameFile.repoName(repoName);
                build();

                // context->threadPool().size(1); // to ease debugging

                // Simulate compilation and linking
                {
                    std::stringstream script;
                    script
                        << "type " << repo.pietH.string() << " > " << pietOut->absolutePath().string()
                        << " & type " << repo.janH.string() << " >> " << pietOut->absolutePath().string()
                        << " & type " << repo.pietCpp.string() << " >> " << pietOut->absolutePath().string() << std::endl;
                    ccPiet->mandatoryOutputs({ pietOut });
                    ccPiet->script(script.str());
                }
                {
                    std::stringstream script;
                    script
                        << "type " << repo.janH.string() << " > " << janOut->absolutePath().string()
                        << " & type " << repo.janCpp.string() << " >> " << janOut->absolutePath().string() << std::endl;

                    ccJan->mandatoryOutputs({ janOut });
                    ccJan->script(script.str());
                }
                {
                    std::stringstream script;
                    linkPietJan->mandatoryOutputs({ pietjanOut });
                    linkPietJan->orderOnlyInputs({ pietOut, janOut });
                    script
                        << "type " << pietOut->absolutePath().string() << " > " << pietjanOut->absolutePath().string()
                        << " & type " << janOut->absolutePath().string() << " >> " << pietjanOut->absolutePath().string() << std::endl;
                    linkPietJan->script(script.str());
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
        }

        ~TestDriver() {
            if (ccPiet == context->nodes().find(ccPiet->name())) {
                context->nodes().remove(ccPiet);
                context->nodes().remove(pietOut);
                context->nodes().remove(ccJan);
                context->nodes().remove(janOut);
                context->nodes().remove(linkPietJan);
                context->nodes().remove(pietjanOut);
            }
        }

        std::shared_ptr<FileRepositoryNode> sourceRepo() {
            return context->findRepository(repoName);
        }

        Node* findNode(std::filesystem::path const& path) {
            auto sptr = context->nodes().find(path);
            return sptr == nullptr ? nullptr : sptr.get();
        }

        void startExecuteRequest(
            std::shared_ptr<BuildRequest> &request,
            std::atomic<std::shared_ptr<BuildResult>>& result,
            Dispatcher& requestDispatcher
        ) {
            auto d = Delegate<void>::CreateLambda([this, &request, &result, &requestDispatcher]() {
                DispatcherFrame frame;
                builder.completor().AddLambda([&result, &frame](std::shared_ptr<BuildResult> r) 
                {
                    result = r;
                    frame.stop();
                });
                builder.start(request);
                context->mainThreadQueue().run(&frame);
                builder.completor().RemoveAll();
                requestDispatcher.stop();
            });
            context->mainThreadQueue().push(std::move(d));
        }

        std::shared_ptr<BuildResult> executeRequest(std::shared_ptr<BuildRequest> request) {
            std::atomic<std::shared_ptr<BuildResult>> result;
            Dispatcher requestDispatcher;
            startExecuteRequest(request, result, requestDispatcher);
            requestDispatcher.run();
            return result;
        }

        void stopBuild() {
            context->mainThreadQueue().push(Delegate<void>::CreateLambda([this]() {builder.stop(); }));
        }

        std::shared_ptr<BuildResult> build() {
            auto request = std::make_shared< BuildRequest>();
            request->repoDirectory(repo.dir);
            request->repoName(repoName);
            return executeRequest(request);
        }

        // Wait for file change event to be received for given paths.
        // When event is received then consume the changes.
        // Return whether event was consumed.
        bool consumeFileChangeEvent(std::initializer_list<std::filesystem::path> paths)
        {
            auto srcFileRepo = sourceRepo();
            std::atomic<bool> received = false;
            Dispatcher dispatcher;
            auto pollChange = Delegate<void>::CreateLambda(
                [&]() {
                    for (auto path : paths) {
                        received = srcFileRepo->hasChanged(path);
                        if (!received) break;
                    }
                    if (received) {
                        srcFileRepo->consumeChanges();
                    }
                    dispatcher.stop();
                });
            const auto retryInterval = std::chrono::milliseconds(100);
            const unsigned int maxRetries = 10;
            unsigned int nRetries = 0;
            do {
                nRetries++;
                dispatcher.start();
                context->mainThreadQueue().push(pollChange);
                dispatcher.run();
                if (!received) std::this_thread::sleep_for(retryInterval);
            } while (nRetries < maxRetries && !received);
            return received;
        }

        std::string expectedPietOutContent() { return repo.pietHContent + repo.janHContent + repo.pietCppContent; }
        std::string expectedJanOutContent() { return repo.janHContent + repo.janCppContent; }
        std::string expectedPietjanOutContent() { return expectedPietOutContent() + expectedJanOutContent();}
        std::string actualPietOutContent() { return readFile(pietOut->absolutePath()); }
        std::string actualJanOutContent() { return readFile(janOut->absolutePath()); }
        std::string actualPietjanOutContent() { return readFile(pietjanOut->absolutePath()); }
    };

    TEST(Builder, firstBuild) {
        TestDriver driver;

        std::shared_ptr<BuildResult> result = driver.build();

        EXPECT_TRUE(result->state() == BuildResult::State::Ok);
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
        auto srcRepo = driver.sourceRepo();
        auto pietCpp = driver.context->nodes().find(srcRepo->symbolicPathOf(driver.repo.pietCpp));
        auto pietH = driver.context->nodes().find(srcRepo->symbolicPathOf(driver.repo.pietH));
        auto janH = driver.context->nodes().find(srcRepo->symbolicPathOf(driver.repo.janH));
        EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), pietCpp));
        EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), pietH));
        EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), janH));

        inputs.clear();
        driver.ccJan->getInputs(inputs);
        EXPECT_EQ(2, inputs.size());
        auto janCpp = driver.context->nodes().find(srcRepo->symbolicPathOf(driver.repo.janCpp));
        EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), janCpp));
        EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), janH));;

        inputs.clear();
        driver.linkPietJan->getInputs(inputs);
        EXPECT_EQ(2, inputs.size());
        EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), driver.pietOut));
        EXPECT_TRUE(inputs.end() != std::find(inputs.begin(), inputs.end(), driver.janOut));

        // Verify nodes started for execution 
        // 2 source file nodes repo.dir/src/pietCpp, repo.dir/src/pietH
        // 2 source file nodes repo.dir/src/janCpp, repo.dir/src/janH
        // 3 generated file nodes pietOut, janOut, pietjanOut before script exec
        // 3 generated file nodes pietOut, janOut, pietjanOut after script exec
        // 3 commandNodes ccPiet, ccJan, linkPietjan
        // 1 groupnodes (__dirtyCommands)
        EXPECT_EQ(14, driver.stats.nStarted); 
        // pietOut, janOut, pietjanOut are started twice
        EXPECT_EQ(11, driver.stats.started.size());
        EXPECT_EQ(0, driver.stats.nDirectoryUpdates);
        // 4 source + 3 generated (before cmd exec) + 3 generated (after cmd exec)
        EXPECT_EQ(10, driver.stats.nRehashedFiles); 

        EXPECT_TRUE(driver.stats.started.contains(driver.findNode(srcRepo->symbolicPathOf(driver.repo.pietCpp))));
        EXPECT_TRUE(driver.stats.started.contains(driver.findNode(srcRepo->symbolicPathOf(driver.repo.pietH))));
        EXPECT_TRUE(driver.stats.started.contains(driver.findNode(srcRepo->symbolicPathOf(driver.repo.janCpp))));
        EXPECT_TRUE(driver.stats.started.contains(driver.findNode(srcRepo->symbolicPathOf(driver.repo.janH))));
        EXPECT_TRUE(driver.stats.started.contains(driver.ccPiet.get()));
        EXPECT_TRUE(driver.stats.started.contains(driver.ccJan.get()));
        EXPECT_TRUE(driver.stats.started.contains(driver.linkPietJan.get()));
        EXPECT_TRUE(driver.stats.started.contains(driver.pietOut.get()));
        EXPECT_TRUE(driver.stats.started.contains(driver.janOut.get()));
        EXPECT_TRUE(driver.stats.started.contains(driver.pietjanOut.get()));

        // Verify nodes self-executed 
        EXPECT_EQ(14, driver.stats.nSelfExecuted);
        EXPECT_EQ(11, driver.stats.selfExecuted.size());
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.findNode(srcRepo->symbolicPathOf(driver.repo.pietCpp))));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.findNode(srcRepo->symbolicPathOf(driver.repo.pietH))));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.findNode(srcRepo->symbolicPathOf(driver.repo.janCpp))));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.findNode(srcRepo->symbolicPathOf(driver.repo.janH))));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.ccPiet.get()));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.ccJan.get()));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.linkPietJan.get()));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.pietOut.get()));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.janOut.get()));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.pietjanOut.get()));
    }

    TEST(Builder, noDirtyNodesAfterConsumeChangesAfterBuild) {
        TestDriver driver;

        driver.build();

        // Assert that the source file repository has detected the changes made
        // by the build to the generated files.
        ASSERT_TRUE(driver.consumeFileChangeEvent({ 
            driver.pietOut->absolutePath(),
            driver.janOut->absolutePath(),
            driver.pietjanOut->absolutePath(),
        }));

        // Because the generated files have not been modified since the last
        // detected file changes we expect all generated files to be still Ok.
        EXPECT_EQ(Node::State::Ok, driver.janOut->state());
        EXPECT_EQ(Node::State::Ok, driver.pietOut->state());
        EXPECT_EQ(Node::State::Ok, driver.pietjanOut->state());

        // And also all other nodes are expected to be still Ok.
        EXPECT_EQ(Node::State::Ok, driver.ccPiet->state());
        EXPECT_EQ(Node::State::Ok, driver.ccJan->state());
        EXPECT_EQ(Node::State::Ok, driver.linkPietJan->state());
        auto srcRepo = driver.sourceRepo();
        
        EXPECT_EQ(Node::State::Ok, driver.findNode(srcRepo->symbolicPathOf(driver.repo.pietCpp))->state());
        EXPECT_EQ(Node::State::Ok, driver.findNode(srcRepo->symbolicPathOf(driver.repo.pietH))->state());
        EXPECT_EQ(Node::State::Ok, driver.findNode(srcRepo->symbolicPathOf(driver.repo.janCpp))->state());
        EXPECT_EQ(Node::State::Ok, driver.findNode(srcRepo->symbolicPathOf(driver.repo.janH))->state());
    }

    TEST(Builder, incrementalBuildWhileNoModifications) {
        TestDriver driver;

        // First build
        driver.build();

        // driver.build() will modify .yam (due to first time write of 
        // persistent buildstate) and generated directory.
        // Do a build to process these changes first.
        std::shared_ptr<BuildResult> result = driver.build();

        result = driver.build();
        EXPECT_TRUE(result->state() == BuildResult::State::Ok);
        EXPECT_EQ(0, driver.stats.nDirectoryUpdates);
        EXPECT_EQ(0, driver.stats.nRehashedFiles);
        EXPECT_EQ(0, driver.stats.started.size());
        EXPECT_EQ(0, driver.stats.selfExecuted.size());
    }
    
    TEST(Builder, incrementalBuildAfterFileModification) {
        TestDriver driver;
        
        // First build
        driver.build();
        driver.stats.reset();

        std::ofstream janSrcFile(driver.repo.janCpp.string());
        janSrcFile << "janjan" << std::endl;
        janSrcFile.close();

        ASSERT_TRUE(driver.consumeFileChangeEvent({ driver.repo.janCpp }));
        EXPECT_EQ(Node::State::Ok, driver.ccPiet->state());
        EXPECT_EQ(Node::State::Dirty, driver.ccJan->state());
        EXPECT_EQ(Node::State::Dirty, driver.linkPietJan->state());
        EXPECT_EQ(Node::State::Ok, driver.pietOut->state());
        EXPECT_EQ(Node::State::Ok, driver.janOut->state());
        EXPECT_EQ(Node::State::Ok, driver.pietjanOut->state());

        // Incremental build
        std::shared_ptr<BuildResult> result = driver.build();
        EXPECT_TRUE(result->state() == BuildResult::State::Ok);
        EXPECT_EQ(Node::State::Ok, driver.ccPiet->state());
        EXPECT_EQ(Node::State::Ok, driver.ccJan->state());
        EXPECT_EQ(Node::State::Ok, driver.linkPietJan->state());
        EXPECT_EQ(Node::State::Ok, driver.pietOut->state());
        EXPECT_EQ(Node::State::Ok, driver.janOut->state());
        EXPECT_EQ(Node::State::Ok, driver.pietjanOut->state());

        // started and selfExecuted also contains 
        // _dirtyCommands from Builder.
        EXPECT_EQ(3, driver.stats.nRehashedFiles);
        EXPECT_EQ(8, driver.stats.started.size());

        // 1: pendingStartSelf of ccJan sees changed hash of janCpp
        // 2: self-execution of ccJan => updates and rehashes janOut 
        // 3: pendingStartSelf of linkPietJan sees changed hash of janOut
        // 4: execution of linkPietJan updates and rehashes pietjanOut
        auto srcRepo = driver.sourceRepo();
        auto janCppNode = driver.context->nodes().find(srcRepo->symbolicPathOf(driver.repo.janCpp));
        EXPECT_EQ(8, driver.stats.selfExecuted.size());
        EXPECT_TRUE(driver.stats.selfExecuted.contains(janCppNode.get()));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.ccJan.get()));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.janOut.get()));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.linkPietJan.get()));
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.pietjanOut.get()));
    }

    TEST(Builder, incrementalBuildAfterFileDeletion) {
        TestDriver driver;

        // First build
        driver.build();
        driver.stats.reset();

        // Delete janCpp, this will fail ccJan (jan.cpp not found).
        std::filesystem::remove(driver.repo.janCpp);

        ASSERT_TRUE(driver.consumeFileChangeEvent({ driver.repo.janCpp }));
        auto srcRepo = driver.sourceRepo();
        auto janCppNode = dynamic_pointer_cast<FileNode>(driver.context->nodes().find(srcRepo->symbolicPathOf(driver.repo.janCpp)));
        auto srcDirNode = dynamic_pointer_cast<DirectoryNode>(driver.context->nodes().find(srcRepo->symbolicPathOf(driver.repo.dir / "src")));
        EXPECT_EQ(Node::State::Ok, driver.ccPiet->state());
        EXPECT_EQ(Node::State::Dirty, srcDirNode->state());
        EXPECT_EQ(Node::State::Dirty, janCppNode->state());
        EXPECT_EQ(Node::State::Dirty, driver.ccJan->state());
        EXPECT_EQ(Node::State::Dirty, driver.linkPietJan->state());
        EXPECT_EQ(Node::State::Ok, driver.pietOut->state());
        EXPECT_EQ(Node::State::Ok, driver.janOut->state());
        EXPECT_EQ(Node::State::Ok, driver.pietjanOut->state());

        // Incremental build
        std::shared_ptr<BuildResult> result = driver.build();
        EXPECT_FALSE(result->state() == BuildResult::State::Ok);
        EXPECT_EQ(Node::State::Ok, driver.ccPiet->state());
        EXPECT_EQ(Node::State::Ok, srcDirNode->state());
        EXPECT_EQ(Node::State::Failed, driver.ccJan->state());
        EXPECT_EQ(Node::State::Deleted, janCppNode->state());
        EXPECT_EQ(Node::State::Canceled, driver.linkPietJan->state());
        EXPECT_EQ(Node::State::Ok, driver.pietOut->state());
        EXPECT_EQ(Node::State::Ok, driver.janOut->state());
        EXPECT_EQ(Node::State::Ok, driver.pietjanOut->state());

        EXPECT_EQ(0, driver.stats.nRehashedFiles); // node in state Deleted is not executed
        EXPECT_EQ(5, driver.stats.selfExecuted.size());
        EXPECT_TRUE(driver.stats.selfExecuted.contains(driver.ccJan.get()));;
    }

    TEST(Builder, stopBuild) {
        TestDriver driver;

        // Set-up command scripts to take ~10 seconds execution time as to
        // have sufficient time to stop a build-in-progress
        auto ping = boost::process::search_path("ping");
        driver.ccPiet->mandatoryOutputs({ });
        driver.ccPiet->script(ping.string() + " -n 10 127.0.0.1");
        driver.ccJan->mandatoryOutputs({ });
        driver.ccJan->script(ping.string() + " -n 10 127.0.0.1");
        driver.linkPietJan->script(ping.string() + " -n 10 127.0.0.1");
        driver.linkPietJan->mandatoryOutputs({ });

        auto request = std::make_shared<BuildRequest>();
        request->repoDirectory(driver.repo.dir);
        request->repoName(driver.repoName);
        std::atomic<std::shared_ptr<BuildResult>> result;
        Dispatcher requestDispatcher;
        driver.startExecuteRequest(request, result, requestDispatcher);
        // wait a bit to get the ping commands running...
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // ...then request build to stop...
        driver.stopBuild();
        // ...and wait for build completion
        requestDispatcher.run();

        EXPECT_EQ(Node::State::Canceled, driver.ccPiet->state());
        EXPECT_EQ(Node::State::Canceled, driver.ccJan->state());
        EXPECT_EQ(Node::State::Canceled, driver.linkPietJan->state());
    }

}

