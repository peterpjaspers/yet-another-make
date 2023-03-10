#include "../BuildClient.h"
#include "../DotYamDirectory.h"
#include "../ConsoleLogBook.h"
#include "../BuildServicePortRegistry.h"
#include "../BuildRequest.h"
#include "../BuildResult.h"
#include "../Dispatcher.h"

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
    auto duration = result.niceDuration();
    std::stringstream ss;
    ss << "Build completed " << (result.succeeded() ? "successfully" : "with errors");
    if (!duration.empty()) ss << " in " << duration;
    ss
        << std::endl
        << "#dirty=" << result.nNodesStarted()
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

// Start yamServer (which hosts the BuildService), iff not yet running.
// Get build service port at which build service is listening.
// Return whether port could be determined, return port in 'port'.
bool startServer(
    ILogBook& logBook, 
    std::filesystem::path const& startDir,
    boost::asio::ip::port_type& port
) {
    const auto pollInterval = 1000ms;
    bool success = true;
    bool startedServer = false;
    BuildServicePortRegistry reader;
    if (!reader.serverRunning()) {
        logStartingServer(logBook);
        auto server = boost::process::search_path("yamServer");
        success = !server.empty();
        if (!success) {
            logCannotFindYamServer(logBook);
        } else {
            startedServer = true;
            boost::process::spawn(server, boost::process::start_dir(startDir.string()));
            std::this_thread::sleep_for(pollInterval);
            reader.read();
        }
    }
    if (success) {
        const int maxRetries = 5;
        int nRetries = 0;
        while (nRetries < maxRetries && !reader.serverRunning()) {
            nRetries += 1;
            std::this_thread::sleep_for(pollInterval);
            reader.read();
        }
        success = reader.serverRunning();
        if (!success) {
            logFailStartServer(logBook);
        } else {
            if (startedServer) logServerStarted(logBook);
            port = reader.port();
        }
    }
    return success;
}

int main(int argc, char argv[]) {
    ConsoleLogBook logBook;
    std::filesystem::path dotYamDir = DotYamDirectory::initialize(std::filesystem::current_path(), &logBook);
    std::filesystem::path repoDir = dotYamDir.parent_path();
    boost::asio::ip::port_type port;
    if (!startServer(logBook, repoDir, port)) return 1;

    BuildClient client(logBook, port);
    auto request = std::make_shared<BuildRequest>();
    request->directory(repoDir);
    std::shared_ptr<BuildResult> result;
    Dispatcher dispatcher;
    client.completor().AddLambda([&](std::shared_ptr<BuildResult> r) {
        result = r;
        dispatcher.push(Delegate<void>::CreateLambda([&dispatcher]() { dispatcher.stop(); }));
    });
    client.startBuild(request);
    dispatcher.run();
    logResult(logBook, *result);
    return result->succeeded() ? 0 : 1;
}