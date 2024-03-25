#include "../BuildClient.h"
#include "../BuildService.h"
#include "../BuildServiceMessageTypes.h"
#include "../MemoryLogBook.h"
#include "../FileSystem.h"
#include "../RepositoryNameFile.h"

#include <atomic>
#include <mutex>
#include <condition_variable>
#include "gtest/gtest.h"

namespace
{
    using namespace YAM;

    BuildResult::State fail = BuildResult::State::Failed;

    class Session {
    private:
        bool _shutdown;
        std::mutex _mutex;
        std::condition_variable _cond;
        std::shared_ptr<BuildResult> _result;

    public:
        MemoryLogBook logBook;
        std::unique_ptr<BuildService> service;
        std::unique_ptr<BuildClient> client;
        std::filesystem::path repoDir;
        std::string repoName;

        Session()
            : service(std::make_unique<BuildService>())
            , client(nullptr)
            , repoDir(FileSystem::createUniqueDirectory())
            , repoName("testRepo")
            , _shutdown(false)
        {
            RepositoryNameFile nameFile(repoDir);
            nameFile.repoName(repoName);
            newClient();
        }

        ~Session() {
            if (!_shutdown) {
                _shutdown = true;
                client->startShutdown();
            }
            service->join(); // service can be joined because it was shutdown
            service = nullptr; // release service to stop watching repoDir
            if (client != nullptr) {
                client->completor().RemoveObject(this);
                client.reset(); // close connection to service
                client = nullptr;
            }
            std::filesystem::remove_all(repoDir);
        }

        std::shared_ptr<BuildResult> shutdown() {
            _shutdown = true;
            if (!client->startShutdown()) return std::make_shared<BuildResult>(fail);
            return wait();
        }

        std::shared_ptr<BuildRequest> buildRequest() {
            auto request = std::make_shared<BuildRequest>();
            request->repoDirectory(repoDir);
            request->repoName(repoName);
            return request;
        }

        std::shared_ptr<BuildResult> init() {
            if (!startBuild(buildRequest())) return std::make_shared<BuildResult>(fail);
            return wait();
        }

        bool startBuild(std::shared_ptr<BuildRequest> request) {
            return client->startBuild(buildRequest());
        }

        std::shared_ptr<BuildResult> build() {
            if (!startBuild(buildRequest())) return std::make_shared<BuildResult>(fail);
            return wait();
        }

        std::shared_ptr<BuildResult> wait() {
            std::unique_lock<std::mutex> lock(_mutex);
            while (_result == nullptr) {
                _cond.wait(lock);
            }
            std::shared_ptr<BuildResult> r = _result;
            _result = nullptr;
            return r;
        }

        void newClient() {
            if (client != nullptr) {
                client->completor().RemoveObject(this);
                client = nullptr;
            }
            client = std::make_unique<BuildClient>(logBook, service->port());
            client->completor().AddLambda([this](std::shared_ptr<BuildResult> r) {
                std::lock_guard<std::mutex> lock(_mutex);
                _result = r;
                _cond.notify_one();
            });
        }        
    };

    TEST(BuildService, constructSession) {
        Session session;
    }

    TEST(BuildService, init) {
        Session session;
        auto result = session.init();
        EXPECT_TRUE(result->state() == BuildResult::State::Ok);
    }

    TEST(BuildService, build) {
        Session session;
        auto result = session.build();
        EXPECT_TRUE(result->state() == BuildResult::State::Ok);
    }

    TEST(BuildService, stopBuild) {
        auto request = std::make_shared<BuildRequest>();
        Session session;

        EXPECT_TRUE(session.startBuild(request));
        session.client->stopBuild();
        auto result = session.wait();
        // A stopBuild may result in successfull or unsuccessfull completion.
        // The former when the build already completed when stop was received.
        // The latter when build in progress was canceled.
        EXPECT_NE(nullptr, result);
    }

    TEST(BuildService, shutdown) {
        Session session;
        auto result = session.shutdown();
        EXPECT_TRUE(result->state() == BuildResult::State::Ok);
    }

    TEST(BuildService, successiveBuilds) {
        Session session;
        auto result = session.build();
        EXPECT_TRUE(result->state() == BuildResult::State::Ok);

        session.newClient();
        result = session.build();
        EXPECT_TRUE(result->state() == BuildResult::State::Ok);
    }

    TEST(BuildService, illegalClientUse) {
        Session session;
        auto result = session.build();
        EXPECT_TRUE(result->state() == BuildResult::State::Ok);

        auto request = std::make_shared<BuildRequest>();
        EXPECT_FALSE(session.startBuild(request));
    }
}