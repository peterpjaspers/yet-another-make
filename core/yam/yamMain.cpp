#include "../BuildClient.h"
#include "../DotYamDirectory.h"
#include "../ConsoleLogBook.h"
#include "../BuildServicePortRegistry.h"
#include "../BuildRequest.h"
#include "../BuildResult.h"
#include "../Dispatcher.h"
#include "../BuildService.h"
#include "../RepositoryNameFile.h"
#include "../BuildOptions.h"
#include "../BuildOptionsParser.h"

#include <thread>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <chrono>
#include <boost/process.hpp>
#include <windows.h> 
#include <stdio.h> 

using namespace YAM;
using namespace std::chrono_literals;

namespace
{
    class CtrlCHandler
    {
    public:
        CtrlCHandler(BuildClient& client);
        ~CtrlCHandler();
        void handleCtrlC();

    private:
        BuildClient& _client;
    };

    CtrlCHandler* handler;
    BOOL WINAPI consoleHandler(DWORD signal) {

        if (signal == CTRL_C_EVENT) {
            printf("Stopping the build\n");
            handler->handleCtrlC();
        }
        return TRUE;
    }

    CtrlCHandler::CtrlCHandler(BuildClient& client) : _client(client) {
        handler = this;
        if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
            printf("\nERROR: Could not add ctrl-C handler");
        }
    }
    CtrlCHandler::~CtrlCHandler() {
        if (!SetConsoleCtrlHandler(consoleHandler, FALSE)) {
            printf("\nERROR: Could not remove ctrl-C handler");
        }
    }

    void CtrlCHandler::handleCtrlC() {
        _client.stopBuild();
    }
}

void logStartingServer(ILogBook& logBook) {
    std::string msg("Starting yamServer");
    LogRecord progress(LogRecord::Aspect::Progress, msg);
    logBook.add(progress);
}

void logConnectingToServer(ILogBook& logBook) {
    std::string msg("Connecting to yamServer...");
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
        << "If yamServer is running: kill yamServer manually with taskmanager" << std::endl
        << "and delete file " << BuildServicePortRegistry::servicePortRegistryPath().string() << " ..." << std::endl
        << "Then retry running yam" << std::endl;
    LogRecord error(LogRecord::Aspect::Error, ss.str());
    logBook.add(error);
}

void logFailConnectServer(ILogBook& logBook, bool shutdown) {
    std::stringstream ss;
    if (shutdown) {
        ss << "Shutdown failed. Please kill yamServer with taskmanager." << std::endl;
    } else {
        ss
            << "Failed to connect to yamServer." << std::endl
            << "If yamServer is running: kill yamServer with taskmanager." << std::endl
            << "Then delete file " << BuildServicePortRegistry::servicePortRegistryPath().string() << "." << std::endl
            << "Then restart yam" << std::endl;
    }
    LogRecord error(LogRecord::Aspect::Error, ss.str());
    logBook.add(error);
}

void logShutdownResult(ILogBook& logBook, BuildResult& result) {
    LogRecord::Aspect aspect;
    std::stringstream ss;
    if (result.state() == BuildResult::State::Ok) {
        aspect = LogRecord::Aspect::Progress;
        ss << "Shutdown completed successfully";
    } else {
        aspect = LogRecord::Aspect::Error;
        ss << "Shutdown failed. Please kill yamServer with taskmanager.";
    }
    LogRecord record(aspect, ss.str());
    logBook.add(record);
}

void logBuildResult(ILogBook& logBook, BuildResult& result) {
    LogRecord::Aspect aspect;
    std::stringstream ss;   
    if (result.state() == BuildResult::State::Ok) {
        if (logBook.warning()) {
            aspect = LogRecord::Aspect::Warning;
            ss << "Build completed with warning(s)";
        } else {
            aspect = LogRecord::Aspect::Progress;
            ss << "Build completed successfully";
        }
    } else if (result.state() == BuildResult::State::Failed) {
        aspect = LogRecord::Aspect::Error;
        ss << "Build completed with errors";
    } else if (result.state() == BuildResult::State::Canceled) {
        if (logBook.error()) {
            aspect = LogRecord::Aspect::Error;
            ss << "Build completed with errors";
        } else {
            aspect = LogRecord::Aspect::Progress;
            ss << "Build canceled by user";
        }
    } else {
        aspect = LogRecord::Aspect::Error;
        ss << "Build completed with unknown result";
    }
    auto duration = result.niceDuration();
    if (!duration.empty()) ss << " in " << duration;
    else ss << " in less than 1 ms ";
    ss
        << std::endl
        << "#started=" << result.nNodesStarted()
        << ", #executed=" << result.nNodesExecuted()
        << ", #dirHashes=" << result.nDirectoryUpdates()
        << ", #fileHashes=" << result.nRehashedFiles() << std::endl;

    LogRecord record(aspect, ss.str());
    logBook.add(record);
}

boost::filesystem::path GetModulePath()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return path;
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
        std::filesystem::path const& startDir,
        bool startServer
    ) {
        boost::asio::ip::port_type port;
        const auto pollInterval = 1000ms;
        bool success = true;
        bool startedServer = false;
        BuildServicePortRegistry portReader;
        if (!portReader.serverRunning() && startServer) {
            logStartingServer(logBook);
            //auto server = boost::process::search_path("yamServer");
            auto server = GetModulePath().parent_path() / "yamServer.exe";
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
            while (startServer && nRetries < maxRetries && !portReader.serverRunning()) {
                logConnectingToServer(logBook);
                nRetries += 1;
                std::this_thread::sleep_for(pollInterval);
                portReader.read();
            }
            success = portReader.serverRunning();
            if (!success) {
                if (startServer) logFailStartServer(logBook);
                else logFailConnectServer(logBook, !startServer);
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

bool yes(std::string const& input) {
    return input == "y" || input == "Y";
}

bool no(std::string const& input) {
    return input == "n" || input == "N";
}

bool confirmRepoDir(std::filesystem::path const& repoDir) {
    std::cout << "Initializing yam on directory " << repoDir << std::endl;
    std::string input;
    do {
        std::cout << "Is this the root of the source code tree that you want to build [y|n]:";
        std::cout << std::endl;
        std::cin >> input;
    } while (!yes(input) && !no(input));
    return yes(input);
}

bool initializeYam(ILogBook &logBook, std::filesystem::path &repoDir, std::string &repoName) {
    std::filesystem::path yamDir = DotYamDirectory::find(std::filesystem::current_path());
    if (yamDir.empty()) {
        yamDir = DotYamDirectory::initialize(std::filesystem::current_path(), &logBook);
    }
    repoDir = yamDir.parent_path();
    if (!repoDir.empty()) {
        RepositoryNameFile repoNameFile(repoDir);
        repoName = repoNameFile.repoName();
        if (repoName.empty()) {
            RepositoryNamePrompt prompt;
            repoName = prompt(repoDir);
            if (!repoName.empty()) {
                repoNameFile.repoName(repoName);
            }
        }
    }
    return !repoName.empty();
}

int main(int argc, char* argv[]) {
    ConsoleLogBook logBook;
    logBook.logElapsedTime(true);
    BuildOptions options;
    BuildOptionsParser parser(argc, argv, options);
    if (parser.parseError()) return 1;
    if (parser.help()) return 0;

    // TODO: remove before release.
    std::vector<LogRecord::Aspect> logAspects = logBook.aspects();
    logAspects.push_back(LogRecord::BuildStateUpdate);
    logAspects.push_back(LogRecord::IgnoredOutputFiles);
    logBook.aspects(logAspects);
    options._logAspects = logAspects;

    std::filesystem::path repoDir;
    std::string repoName;
    bool success = initializeYam(logBook, repoDir, repoName);

    bool shutdown = parser.shutdown();
    bool inProcess = parser.noServer() && !shutdown;
    std::shared_ptr<ServerAccess> server;
    if (success) {
        if (inProcess) {
            server = std::make_shared<InProcessServer>(logBook, repoDir);
        } else {
            server = std::make_shared<OutProcessServer>(logBook, repoDir, !shutdown);
        }
        success = server->connected();
    }
    if (success) {
        std::shared_ptr<BuildResult> result;
        Dispatcher dispatcher;
        BuildClient& client = server->client();
        client.completor().AddLambda([&](std::shared_ptr<BuildResult> r) {
            result = r;
            dispatcher.push(Delegate<void>::CreateLambda([&dispatcher]() { 
                dispatcher.stop();
            }));
        });
        if (shutdown) {
            client.startShutdown();
            dispatcher.run();
            logShutdownResult(logBook, *result);
        } else {
            auto request = std::make_shared<BuildRequest>();
            request->repoDirectory(repoDir);
            request->repoName(repoName);
            request->options(options);
            client.startBuild(request);
            CtrlCHandler handler(client);
            dispatcher.run();
            if (parser.noServer()) client.startShutdown();
            logBuildResult(logBook, *result);
        }    
        success = result->state() == BuildResult::State::Ok;
    }
    return success ? 0 : 1;
}