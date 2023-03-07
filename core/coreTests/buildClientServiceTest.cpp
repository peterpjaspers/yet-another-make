#include "../BuildClient.h"
#include "../BuildService.h"
#include "../BuildServiceMessageTypes.h"
#include "../MemoryLogBook.h"
#include "../FileSystem.h"

#include <atomic>
#include <mutex>
#include <condition_variable>
#include "gtest/gtest.h"

namespace
{
    using namespace YAM;

    class Session {
    public:
        MemoryLogBook logBook;
        std::shared_ptr<BuildService> service;
        std::shared_ptr<BuildClient> client;
        std::filesystem::path repoDir;

        Session()
            : service(std::make_shared<BuildService>(false))
            , client(std::make_shared<BuildClient>(logBook, service->port()))
            , repoDir(FileSystem::createUniqueDirectory())
            , _shutdown(false)
        {
            client->completor().AddLambda([&](std::shared_ptr<BuildResult> r) {
                std::lock_guard<std::mutex> lock(_mutex);
                _result = r;
                _cond.notify_one();
            });       
        }

        ~Session() {
            if (!_shutdown) {
                _shutdown = true;
                client->shutdown();
            }
            service->join(); // service can be joined because it was shutdown
            client.reset(); // close connection to service
        }

        std::shared_ptr<BuildResult> shutdown() {
            _shutdown = true;
            client->shutdown();
            return wait();
        }

        std::shared_ptr<BuildResult> init() {
            auto request = std::make_shared<BuildRequest>(BuildRequest::RequestType::Init);
            startBuild(request);
            return wait();
        }

        void startBuild(std::shared_ptr<BuildRequest> request) {
            request->directory(repoDir);
            client->startBuild(request);
        }

        std::shared_ptr<BuildResult> build() {
            auto request = std::make_shared<BuildRequest>(BuildRequest::RequestType::Build);
            startBuild(request);
            return wait();
        }

        std::shared_ptr<BuildResult> clean() {
            auto request = std::make_shared<BuildRequest>(BuildRequest::RequestType::Clean);
            startBuild(request);
            return wait();
        }

        std::shared_ptr<BuildResult> wait() {
            std::unique_lock<std::mutex> lock(_mutex);
            while (_result == nullptr) {
                _cond.wait(lock);
            }
            return _result;
        }

    private:
        bool _shutdown;
        std::mutex _mutex;
        std::condition_variable _cond;
        std::shared_ptr<BuildResult> _result;

    };

    TEST(BuildService, init) {
        Session session;
        auto result = session.init();
        EXPECT_TRUE(result->succeeded());
    }

    TEST(BuildService, build) {
        Session session;
        auto result = session.build();
        EXPECT_TRUE(result->succeeded());
    }

    TEST(BuildService, clean) {
        Session session;
        auto result = session.clean();
        EXPECT_TRUE(result->succeeded());
    }

    TEST(BuildService, shutdown) {
        Session session;
        auto result = session.shutdown();
        EXPECT_TRUE(result->succeeded());
    }
}