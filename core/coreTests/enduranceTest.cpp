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

    void writeFile(std::filesystem::path p, std::string const& content) {
        std::ofstream stream(p.string().c_str());
        EXPECT_TRUE(stream.is_open());
        stream << content;
        stream.close();
    }

    std::string readFile(std::filesystem::path path) {
        std::ifstream file(path.string());
        std::string line, content;
        while (getline(file, line)) content += line;
        file.close();
        return content;
    }

    class TestDriver
    {
    public:
        std::string repoName;
        std::filesystem::path repoDir;
        Builder builder;
        ExecutionContext* context;

        TestDriver()
            : repoName("test_0")
            , repoDir("D:\\test_yam")
            , context(builder.context())
        {
            std::vector<LogRecord::Aspect> logAspects;
            logAspects.push_back(LogRecord::Error);
            logAspects.push_back(LogRecord::Warning);
            logAspects.push_back(LogRecord::Progress);
            logAspects.push_back(LogRecord::BuildStateUpdate);
            context->logBook()->aspects(logAspects);
            // make test a bit more deterministic
            context->threadPool().size(1);
        }

        ~TestDriver() {
        }

        void startExecuteRequest(
            std::shared_ptr<BuildRequest>& request,
            std::atomic<std::shared_ptr<BuildResult>>& result,
            Dispatcher& requestDispatcher
        ) {
            auto d = Delegate<void>::CreateLambda([this, &request, &result, &requestDispatcher]() {
                DispatcherFrame frame;
            builder.completor().AddLambda(
                [&result, &frame](std::shared_ptr<BuildResult> r) {
                result = r;
            frame.stop();
            }
            );
            builder.start(request);
            context->mainThreadQueue().run(&frame);
            builder.completor().RemoveAll();
            requestDispatcher.stop();
            }
            );
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
            context->mainThreadQueue().push(
                Delegate<void>::CreateLambda([this]() {builder.stop(); }));
        }

        std::shared_ptr<BuildResult> build() {
            auto request = std::make_shared< BuildRequest>();
            request->repoDirectory(repoDir);
            request->repoName(repoName);
            return executeRequest(request);
        }

        void addRepos() {
            std::filesystem::path reposFile(repoDir / "yamConfig/repositories.txt");
            std::string repos = R"(
                name = test_1 dir = ..\test_yam_1 type = Build;
            )";
            writeFile(reposFile, repos);
        }

        void removeRepos() {
            std::filesystem::path reposFile(repoDir / "yamConfig/repositories.txt");
            writeFile(reposFile, "");
        }
    };

    // Reproduces crash in build 2.0
    //TEST(Endurance, repeatAddRemoveRepositories_crashIn2_0) {
    void ignore2_0() {
        std::filesystem::remove_all("D:\\test_yam");
        std::filesystem::remove_all("D:\\test_yam_1");
        std::filesystem::copy("D:\\clean_repos\\test_yam", "D:\\test_yam", std::filesystem::copy_options::recursive);
        std::filesystem::copy("D:\\clean_repos\\test_yam_1", "D:\\test_yam_1", std::filesystem::copy_options::recursive);
        for (int nRestarts = 0; nRestarts < 10; nRestarts++) {
            TestDriver driver;
            auto& stats = driver.context->statistics();
            stats.registerNodes = true;
            driver.removeRepos();
            for (int nBuilds = 0; nBuilds < 2; nBuilds++) {
                stats.reset();
                std::cout << "\nStarting build " << nRestarts << "." << nBuilds << std::endl;
                std::shared_ptr<BuildResult> result = driver.build();
                ASSERT_EQ(TRUE, result->state() == BuildResult::State::Ok);
                std::cout << "Completed build " << nRestarts << "." << nBuilds << std::endl;
                if (nBuilds % 2 == 0) {
                    driver.addRepos();
                } else {
                    driver.removeRepos();
                }
            }
        }
    }

    // Reproduces crash in build 4.0
    //TEST(Endurance, repeatAddRemoveRepositories_crashIn4_0) {
    void ignore4_0() {
        std::filesystem::remove_all("D:\\test_yam");
        std::filesystem::remove_all("D:\\test_yam_1");
        std::filesystem::copy("D:\\clean_repos\\test_yam", "D:\\test_yam", std::filesystem::copy_options::recursive);
        std::filesystem::copy("D:\\clean_repos\\test_yam_1", "D:\\test_yam_1", std::filesystem::copy_options::recursive);
        for (int nRestarts = 0; nRestarts < 10; nRestarts++) {
            TestDriver driver;
            auto& stats = driver.context->statistics();
            stats.registerNodes = true;
            if (nRestarts % 2 == 0) {
                driver.removeRepos();
            } else {
                driver.addRepos();
            }
            for (int nBuilds = 0; nBuilds < 1; nBuilds++) {
                stats.reset();
                std::cout << "\nStarting build " << nRestarts << "." << nBuilds << std::endl;
                std::shared_ptr<BuildResult> result = driver.build();
                ASSERT_EQ(TRUE, result->state() == BuildResult::State::Ok);
                std::cout << "Completed build " << nRestarts << "." << nBuilds << std::endl;
            }
        }
    }
}
