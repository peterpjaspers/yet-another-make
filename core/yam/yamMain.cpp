#include "../BuildClient.h"
#include "../DotYamDirectory.h"
#include "../ConsoleLogBook.h"
#include "../BuildServicePortRegistry.h"
#include "../BuildRequest.h"
#include "../BuildResult.h"
#include "../Dispatcher.h"
#include "../BuildService.h"

#include <thread>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <chrono>
#include <boost/process.hpp>

using namespace YAM;
using namespace std::chrono_literals;

void logStartingServer(ILogBook& logBook) {
    std::string msg("Starting yamServer");
    LogRecord progress(LogRecord::Aspect::Progress, msg);
    logBook.add(progress);
}

void logServerStarted(ILogBook& logBook) {
    std::string msg("Started yamServer");
    LogRecord progress(LogRecord::Aspect::Progress, msg);
    logBook.add(progress);
}

void logFailStartServer(ILogBook& logBook) {
    std::stringstream ss;
    ss
        << "Cannot verify that yamServer is running" << std::endl
        << "If yamServer is running: kill yamServer ... " << std::endl
        << "Delete file " << BuildServicePortRegistry::servicePortRegistryPath().string() << " ..." << std::endl
        << "Retry running yam" << std::endl;
    LogRecord error(LogRecord::Aspect::Error, ss.str());
    logBook.add(error);
}

void logResult(ILogBook& logBook, BuildResult& result) {
    std::stringstream ss;
    ss << "Build completed " << (result.succeeded() ? "successfully" : "with errors");
    //auto duration = result.niceDuration();
    //if (!duration.empty()) ss << " in " << duration;
    //else ss << " in less than 1 ms ";
    ss
        << std::endl
        << "#started=" << result.nNodesStarted()
        << ", #executed=" << result.nNodesExecuted()
        << ", #dirHashes=" << result.nDirectoryUpdates()
        << ", #fileHashes=" << result.nRehashedFiles() << std::endl;

    LogRecord::Aspect aspect = result.succeeded() ? LogRecord::Aspect::Progress : LogRecord::Aspect::Error;
    LogRecord progress(aspect, ss.str());
    logBook.add(progress);
}
void logCannotFindYamServer(ILogBook& logBook) {
    std::stringstream ss;
    ss
        << "Can not find yamServer executable file." << std::endl
        << "Fix: adjust PATH environment variable to include directory that contains yamServer."
        << std::endl;

    LogRecord progress(LogRecord::Aspect::Error, ss.str());
    logBook.add(progress);
}

class ServerAccess {
    protected:
        std::shared_ptr<BuildClient> _client;
        bool _connected;

public:
    BuildClient& client() { return *_client; }
    bool connected() { return _connected; }
};

class OutProcessServer : public ServerAccess 
{
public:
    OutProcessServer(
        ILogBook& logBook,
        std::filesystem::path const& startDir        
    ) {
        boost::asio::ip::port_type port;
        const auto pollInterval = 1000ms;
        bool success = true;
        bool startedServer = false;
        BuildServicePortRegistry portReader;
        if (!portReader.serverRunning()) {
            logStartingServer(logBook);
            auto server = boost::process::search_path("yamServer");
            success = !server.empty();
            if (!success) {
                logCannotFindYamServer(logBook);
            } else {
                startedServer = true;
                boost::process::spawn(server, boost::process::start_dir(startDir.string()));
                std::this_thread::sleep_for(pollInterval);
                portReader.read();
            }
        }
        if (success) {
            const int maxRetries = 5;
            int nRetries = 0;
            while (nRetries < maxRetries && !portReader.serverRunning()) {
                nRetries += 1;
                std::this_thread::sleep_for(pollInterval);
                portReader.read();
            }
            success = portReader.serverRunning();
            if (!success) {
                logFailStartServer(logBook);
            } else {
                if (startedServer) logServerStarted(logBook);
                port = portReader.port();
                _client = std::make_shared<BuildClient>(logBook, port);
            }
        }
        _connected = success;
    }
};

class InProcessServer : public ServerAccess {
private:
    BuildService _service;

public:
    InProcessServer(
        ILogBook& logBook,
        std::filesystem::path const& startDir
    ) {
        boost::asio::ip::port_type port = _service.port(); 
        _client = std::make_shared<BuildClient>(logBook, port);
        _connected = true;
    }

    ~InProcessServer() {
        _service.join();
    }
};

int main(int argc, char argv[]) {
    ConsoleLogBook logBook;
    logBook.logElapsedTime(true);
    //auto allAspects = LogRecord::allAspects();
    //allAspects.push_back(LogRecord::BuildState);
    //logBook.setAspects(allAspects);

    std::filesystem::path dotYamDir = DotYamDirectory::initialize(std::filesystem::current_path(), &logBook);
    std::filesystem::path repoDir = dotYamDir.parent_path();
    bool inProcess = false;
    std::shared_ptr<ServerAccess> server;

    if (inProcess) {
        server = std::make_shared<InProcessServer>(logBook, repoDir);
    } else {
        server = std::make_shared<OutProcessServer>(logBook, repoDir);
    }
    if (!server->connected()) return 1;

    auto request = std::make_shared<BuildRequest>();
    request->directory(repoDir);
    std::shared_ptr<BuildResult> result;
    Dispatcher dispatcher;
    BuildClient& client = server->client();
    client.completor().AddLambda([&](std::shared_ptr<BuildResult> r) {
        result = r;
        dispatcher.push(Delegate<void>::CreateLambda([&dispatcher]() { dispatcher.stop(); }));
    });
    client.startBuild(request);
    dispatcher.run();
    if (inProcess) client.startShutdown();
    logResult(logBook, *result);
    return result->succeeded() ? 0 : 1;
}