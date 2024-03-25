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
        , _state(Idle)
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

    bool BuildClient::startBuild(std::shared_ptr<BuildRequest> request) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_state != Idle) return false;

        _state = Building;
        auto msg = dynamic_pointer_cast<IStreamable>(request);
        _protocol->send(msg);
        return true;
    }

    bool BuildClient::stopBuild() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_state != Building) return false;

        _state = StoppingBuild;
        auto msg = dynamic_pointer_cast<IStreamable>(std::make_shared<StopBuildRequest>());
        _protocol->send(msg);
        return true;
    }

    bool BuildClient::startShutdown() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_state != Idle && _state != Done) return false;

        _state = _state == Done ? Done : Shuttingdown;
        auto msg = dynamic_pointer_cast<IStreamable>(std::make_shared<ShutdownRequest>());
        _protocol->send(msg);
        return _state == Shuttingdown;
    }

    // Return the completor delegate used to notify build completion.
    MulticastDelegate<std::shared_ptr<BuildResult>>& BuildClient::completor() {
        return _completor;
    }
    
    void BuildClient::run() {
        std::shared_ptr<BuildResult> result;
        try {
            bool done = false;
            while (!done) {
                std::shared_ptr<IStreamable> msg = _protocol->receive();
                result = dynamic_pointer_cast<BuildResult>(msg);
                auto logRecord = dynamic_pointer_cast<LogRecord>(msg);
                if (logRecord != nullptr) {
                    _logBook.add(*logRecord);
                } else {
                    done = true;
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
        {
            if (result == nullptr) {
                result = std::make_shared<BuildResult>(BuildResult::State::Failed);
            }
            std::lock_guard<std::mutex> lock(_mutex);
            _state = Done;
        }
        _completor.Broadcast(result);
    }
}