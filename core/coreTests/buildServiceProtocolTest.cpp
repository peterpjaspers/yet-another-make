#include "../BuildServiceProtocol.h"
#include "../BuildServiceMessageTypes.h"
#include "../MemoryStream.h"
#include "../LogRecord.h"

#include <memory>

#include "gtest/gtest.h"

namespace
{
    using namespace YAM;

    class ProtocolSetup
    {
    public:
        ProtocolSetup()
            : buildRequest(std::make_shared<BuildRequest>())
            , stopBuildRequest(std::make_shared<StopBuildRequest>())
            , shutdownRequest(std::make_shared<ShutdownRequest>())
            , logRecord(std::make_shared<LogRecord>(LogRecord::Aspect::Progress, std::string("test")))
            , buildResult(std::make_shared<BuildResult>())
            , toServiceStream(std::make_shared<MemoryStream>())
            , toClientStream(std::make_shared<MemoryStream>())
            , client(toClientStream, toServiceStream, true)
            , service(toServiceStream, toClientStream, false)
        {}

        void sendToService(std::shared_ptr<IStreamable> msg) {
            client.send(msg);
        }

        void sendToClient(std::shared_ptr<IStreamable> msg) {
            service.send(msg);
        }

        std::shared_ptr<IStreamable> receiveFromService() {
            auto msg = client.receive();
            return msg;
        }
        std::shared_ptr<IStreamable> receiveFromClient() {
            auto msg = service.receive();
            return msg;
        }

        std::shared_ptr<BuildRequest> buildRequest;
        std::shared_ptr<StopBuildRequest> stopBuildRequest;
        std::shared_ptr<ShutdownRequest> shutdownRequest;
        std::shared_ptr<LogRecord> logRecord;
        std::shared_ptr<BuildResult> buildResult;
        std::shared_ptr<MemoryStream> toServiceStream;
        std::shared_ptr<MemoryStream> toClientStream;
        BuildServiceProtocol client;
        BuildServiceProtocol service;
    };

    TEST(BuildServiceProtocol, Build) {
        ProtocolSetup setup;
        setup.client.send(setup.buildRequest);
        auto request = dynamic_pointer_cast<BuildRequest>(setup.service.receive());
        ASSERT_NE(nullptr, request);
        setup.service.send(setup.buildResult);
        auto result = dynamic_pointer_cast<BuildResult>(setup.client.receive());
        ASSERT_NE(nullptr, result);
    }

    TEST(BuildServiceProtocol, StopBuild) {
        ProtocolSetup setup;
        setup.client.send(setup.buildRequest);
        auto request = dynamic_pointer_cast<BuildRequest>(setup.service.receive());
        ASSERT_NE(nullptr, request);

        setup.service.send(setup.logRecord);
        setup.service.send(setup.logRecord);
        auto record = dynamic_pointer_cast<LogRecord>(setup.client.receive());
        ASSERT_NE(nullptr, record);
        record = dynamic_pointer_cast<LogRecord>(setup.client.receive());
        ASSERT_NE(nullptr, record);

        setup.client.send(setup.stopBuildRequest);
        auto stopRequest = dynamic_pointer_cast<StopBuildRequest>(setup.service.receive());
        setup.service.send(setup.buildResult);
        auto result = dynamic_pointer_cast<BuildResult>(setup.client.receive());
        ASSERT_NE(nullptr, result);
    }

    TEST(BuildServiceProtocol, Shutdown) {
        ProtocolSetup setup;
        setup.client.send(setup.shutdownRequest);
        auto request = dynamic_pointer_cast<ShutdownRequest>(setup.service.receive());
        ASSERT_NE(nullptr, request);
        setup.service.send(setup.buildResult);
        auto result = dynamic_pointer_cast<BuildResult>(setup.client.receive());
        ASSERT_NE(nullptr, result);
    }

    TEST(BuildServiceProtocol, ClientViolation) {
        ProtocolSetup setup;
        EXPECT_THROW(setup.client.send(setup.logRecord), std::exception);
    }

    TEST(BuildServiceProtocol, ServiceViolation) {
        ProtocolSetup setup;
        EXPECT_THROW(setup.service.send(setup.buildRequest), std::exception);
    }
}