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
        std::string repoName;
        std::filesystem::path repoDir;
        Builder builder;
        ExecutionContext* context;

        TestDriver()
            : repoName("test_0")
            , repoDir("D:\\test_yam")
            , context(builder.context())
        {
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
                name = test_1 dir = ..\test_yam_1 type = Integrated;
                name = test_2 dir = ..\test_yam_2 type = Integrated; 
            )";
            writeFile(reposFile, repos);
        }

        void removeRepos() {
            std::filesystem::path reposFile(repoDir / "yamConfig/repositories.txt");
            writeFile(reposFile, "");
        }
    };

    TEST(Endurance, repeatAddRemoveRepositories) {
    //void ignore() {
        std::filesystem::remove_all("D:\\test_yam");
        std::filesystem::remove_all("D:\\test_yam_1");
        std::filesystem::remove_all("D:\\test_yam_2");
        std::filesystem::copy("D:\\clean_repos\\test_yam",   "D:\\test_yam",   std::filesystem::copy_options::recursive);
        std::filesystem::copy("D:\\clean_repos\\test_yam_1", "D:\\test_yam_1", std::filesystem::copy_options::recursive);
        std::filesystem::copy("D:\\clean_repos\\test_yam_2", "D:\\test_yam_2", std::filesystem::copy_options::recursive);
        for (int nRestarts = 0; nRestarts < 100; nRestarts++) {
            TestDriver driver;
            auto &stats = driver.context->statistics();
            stats.registerNodes = true;
            driver.removeRepos();
            for (int nBuilds = 0; nBuilds < 2; nBuilds++) {
                stats.reset();
                std::shared_ptr<BuildResult> result = driver.build();
                ASSERT_EQ(TRUE, result->succeeded());
                std::cout << "Restarts=" << nRestarts << ", builds=" << nBuilds << std::endl;
                if (nBuilds%2 == 0) {
                    driver.addRepos();
                } else {
                    driver.removeRepos();
                }
            }
        }
    }
}

