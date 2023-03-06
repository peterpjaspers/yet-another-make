#pragma once

#include "ILogBook.h"
#include "Delegates.h"
#include "Dispatcher.h"

#include <memory>
#include <thread>
#include <boost/asio.hpp>

namespace YAM
{
    class TcpStream;
    class BuildRequest;
    class BuildResult;
    class BuildServiceProtocol;

    class __declspec(dllexport) BuildClient
    {
    public:
        // Construct a client that connects to the BuildService associated
        // with the .yam directory.
        // Search the .yam directory in the current directory and its parent
        // directories. If no .yam directory is found: create .yam directory 
        // in the current directory.
        // If no service is running yet: start the service.
        // If servicePort == 0: connect to the service using the port number 
        // published by the service in file .yam/.servicePort. 
        // If servicePort != 0: connect to the service using servicePort.
        BuildClient(ILogBook& logBook, boost::asio::ip::port_type servicePort = 0);

        // Start a build, call completer().Broadcast(buildReply) when the build 
        // has finished.
        void startBuild(std::shared_ptr<BuildRequest> request);

        // Stop the build, call completer().Execute(buildReply) when the build 
        // has stopped.
        void stopBuild();

        // Shutdown the service, call completer().Execute(nullptr) when service
        // acknowledged shutdown.
        void shutdown();
        
        // block caller until build has completed.
        void waitForCompletion();

        // Return the completor delegate used to notify build completion.
        MulticastDelegate<std::shared_ptr<BuildResult>>& completor();
        
    private:
        void run();

        ILogBook& _logBook;
        boost::asio::io_context _context;
        boost::asio::ip::tcp::socket _socket;
        std::shared_ptr<TcpStream> _service;
        std::shared_ptr<BuildServiceProtocol> _protocol;
        Dispatcher _dispatcher;
        std::shared_ptr<std::thread> _receiver;
        MulticastDelegate<std::shared_ptr<BuildResult>> _completor;
    };
}


