#pragma once

#include "ILogBook.h"
#include "Delegates.h"

#include <memory>
#include <thread>
#include <mutex>
#include <boost/asio.hpp>

namespace YAM
{
    class TcpStream;
    class BuildRequest;
    class BuildResult;
    class BuildServiceProtocol;

    // A build client connects to a build service.
    // A client implements the following state diagram:
    // 
    // +---------------|-----------------+--------------------+-----------------+
    // |State          |  Event          |  Action            |  Next state     |
    // +---------------+-----------------+--------------------+-----------------+
    // | Idle          | startBuild()    |  send request      |  Building       |
    // | Building      | stopBuild()     |  send request      |  StoppingBuild  |
    // | StoppingBuild | stopBuild()     |        -           |  StoppingBuild  |
    // | Building      | async result    |  notify completion |  Done           |
    // | StoppingBuild | async result    |  notify completion |  Done           |
    // |               |                 |                    |                 |
    // | Idle          | startShutdown() |  send request      |  ShuttingDown   |
    // | ShuttingDown  | async result    |  notify completion |  Done           |
    // |               |                 |                    |                 |
    // | Done*         | startShutdown() |  send request      |  Done           |
    // | Done          | startBuild()    |  return false      |  Done           |
    // | Done          | stopBuild()     |  return false      |  Done           |
    // +---------------+-----------------+--------------------+-----------------+
    // 
    // send request : synchronous send of a request message to service.
    // async result : asynchronous receipt of a result message from service.
    // notify completion : completor().Broadcast(result)
    // Done* : in this case no completion notification will happen.
    //  
    class __declspec(dllexport) BuildClient
    {
    public:
        // Construct a client that connects to the BuildService associated
        // with the .yam directory.
        // Search the .yam directory. If no .yam directory is found: create
        // .yam directory in the current directory.
        // If no service is running yet: start the service.
        // If servicePort == 0: connect to the service using the port number 
        // published by the service in file .yam/.servicePort. 
        // If servicePort != 0: connect to the service using servicePort.
        BuildClient(ILogBook& logBook, boost::asio::ip::port_type servicePort = 0);

        ~BuildClient();

        // Start a build, call completer().Broadcast(buildReply) when the build 
        // has finished.
        // Return whether build was started.
        bool startBuild(std::shared_ptr<BuildRequest> request);

        // Stop the build, call completer().Execute(buildReply) when the build 
        // has stopped.
        // Return whether stop build was started.
        bool stopBuild();

        // Start shutdown the build service, call completer().Execute(nullptr)
        // when service acknowledged shutdown.
        // Return whether shutdown was started.
        bool startShutdown();

        // Return the completor delegate used to broadcast build completion.
        // Take care: broadcast is done in another thread than thread that 
        // constructed the build client.
        MulticastDelegate<std::shared_ptr<BuildResult>>& completor();
        
    private:
        void run();

        enum State { Idle, Building, StoppingBuild, Shuttingdown, Done  };

        ILogBook& _logBook;
        boost::asio::io_context _context;
        boost::asio::ip::tcp::socket _socket;
        std::shared_ptr<TcpStream> _service;
        std::shared_ptr<BuildServiceProtocol> _protocol;
        std::mutex _mutex;
        State _state;
        std::shared_ptr<std::thread> _receiver;
        MulticastDelegate<std::shared_ptr<BuildResult>> _completor;
    };
}


