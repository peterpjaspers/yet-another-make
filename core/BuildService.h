#pragma once

#include "Dispatcher.h"
#include "ILogBook.h"
#include "Builder.h"

#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <boost/asio.hpp>

namespace YAM
{
    class TcpStream;
    class BuildResult;
    class IStreamable;
    class BuildServiceProtocol;

    // A build service accepts a tcp/ip connection from the build client. 
    // Only one client at-a-time can connect.
    // Service adheres to BuildServiceProtocol.
    //
    class __declspec(dllexport) BuildService : public ILogBook
    {
    public:
        // Run a build service that accepts client connections at a
        // dynamically allocated tcp port.
        BuildService();
        virtual ~BuildService();

        // Return the port at which the service accepts client connections.
        boost::asio::ip::port_type port() const;

        // Wait for the service to be shutdown.
        void join();

        // Inherited via ILogBook
        void add(LogRecord const& record) override;

    private:
        void run();
        void postRequest(std::shared_ptr<IStreamable> request);
        void handleRequest(std::shared_ptr<IStreamable> request);
        void handleBuildCompletion(std::shared_ptr<BuildResult> result);
        void send(std::shared_ptr<IStreamable> msg);
        void closeClient();

        class BuildServiceLogBook : public ILogBook {
        public:
            BuildServiceLogBook(ILogBook* logBook) : _logBook(logBook) {}
            void add(LogRecord const& record) override {
                _logBook->add(record);
            }
        private:
            ILogBook* _logBook;
        };

        boost::asio::io_context _context;
        boost::asio::ip::tcp::endpoint _service;
        boost::asio::ip::tcp::acceptor _acceptor;
        std::shared_ptr<BuildServiceLogBook> _logBook;
        Builder _builder;
        std::thread _serviceThread;
        std::mutex _connectMutex;
        std::mutex _logMutex;
        std::shared_ptr<TcpStream> _client;
        std::shared_ptr<BuildServiceProtocol> _protocol;
    };
}

