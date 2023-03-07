#include "BuildClient.h"
#include "BuildServiceProtocol.h"
#include "BuildServiceMessageTypes.h"
#include "TcpStream.h"
#include "EndOfStreamException.h"

#include <string>
#include <sstream>

namespace YAM
{
    BuildClient::BuildClient(ILogBook& logBook, boost::asio::ip::port_type servicePort)
        : _logBook(logBook)
        , _socket(_context)
    {
        // Construct a client that connects to the BuildService associated
        // with the .yam directory.
        // Search the .yam directory in the current directory and its parent
        // directories. If no .yam directory is found: create .yam directory 
        // in the current directory.
        // If no service is running yet: start the service.
        // Connect to the service using the port number published by the service
        // in file .yam/.servicePort.
        if (servicePort != 0) {
            std::stringstream ss;
            ss << servicePort;
            boost::asio::ip::tcp::resolver resolver(_context);
            boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", ss.str());
            boost::asio::connect(_socket, endpoints);
            _service = std::make_shared<TcpStream>(_socket);
            _protocol = std::make_shared<BuildServiceProtocol>(_service, _service, true);
            _receiver = std::make_shared<std::thread>(&BuildClient::run, this);
        }
    }

    BuildClient::~BuildClient() {
        _receiver->join();
        _socket.close();
    }

    // Start a build, call completer().Execute(buildReply) when the build 
    // has finished.
    void BuildClient::startBuild(std::shared_ptr<BuildRequest> request) {
        auto msg = dynamic_pointer_cast<IStreamable>(request);
        _protocol->send(msg);
    }

    // Stop the build, call completer().Execute(buildReply) when the build 
    // has stopped.
    void BuildClient::stopBuild() {
        auto msg = dynamic_pointer_cast<IStreamable>(std::make_shared<StopBuildRequest>());
        _protocol->send(msg);
    }

    // Shutdown the service, call completer().Execute(nullptr) when service
    // acknowledged shutdown.
    void BuildClient::shutdown() {
        auto msg = dynamic_pointer_cast<IStreamable>(std::make_shared<ShutdownRequest>());
        _protocol->send(msg);
    }

    // Return the completor delegate used to notify build completion.
    MulticastDelegate<std::shared_ptr<BuildResult>>& BuildClient::completor() {
        return _completor;
    }
    
    void BuildClient::run() {
        try {
            bool done = false;
            while (!done) {
                std::shared_ptr<IStreamable> msg = _protocol->receive();
                auto result = dynamic_pointer_cast<BuildResult>(msg);
                auto logRecord = dynamic_pointer_cast<LogRecord>(msg);
                if (result != nullptr) {
                    _completor.Broadcast(result);
                    done = true;
                } else {
                    _logBook.add(*logRecord);
                }
            }
        } catch (std::exception e) {
            _logBook.add(LogRecord(
                LogRecord::Aspect::Progress,
                std::string("lost connection to service: ") + e.what())
            );
        } catch (EndOfStreamException e) {
            _logBook.add(LogRecord(
                LogRecord::Aspect::Progress,
                std::string("lost connection to service: ") + e._message)
            );
        }
    }
}