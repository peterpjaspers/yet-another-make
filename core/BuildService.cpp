#include "BuildService.h"
#include "TcpStream.h"
#include "BuildServiceProtocol.h"
#include "BuildServiceMessageTypes.h"
#include "BuildServicePortRegistry.h"

#include <string>

namespace YAM
{
    BuildService::BuildService()
        : _service(boost::asio::ip::tcp::v4(), 0)
        , _acceptor(_context, _service)
        , _shutdown(false)
        , _thread(&BuildService::run, this)
    {}

    boost::asio::ip::port_type BuildService::port() const {
        return _acceptor.local_endpoint().port();
    }

    void BuildService::join() {
        _thread.join();
    }

    void BuildService::run() {
        while (!_shutdown) {
            boost::asio::ip::tcp::socket socket(_context);
            try {
                _acceptor.accept(socket);
                {
                    std::lock_guard<std::mutex> lock(_connectMutex);
                    _client = std::make_shared<TcpStream>(socket);
                    _protocol = std::make_shared<BuildServiceProtocol>(_client, _client, false);
                }
                std::shared_ptr<IStreamable> request = _protocol->receive();
                while (!_shutdown && request != nullptr) {
                    handleRequest(request);
                    if (!_shutdown) request = _protocol->receive();
                }
            } catch (std::exception& e) {
            } catch (EndOfStreamException e) {
            }
            closeClient();
        }
    }

    void BuildService::add(LogRecord const& record) {
        std::lock_guard<std::mutex> lock(_logMutex);
        ILogBook::add(record);
        auto ptr = std::make_shared<LogRecord>(record);
        send(ptr);
    }

    void BuildService::handleRequest(std::shared_ptr<IStreamable> request) {
        auto buildRequest = dynamic_pointer_cast<BuildRequest>(request);
        auto stopRequest = dynamic_pointer_cast<StopBuildRequest>(request);
        auto shutdownRequest = dynamic_pointer_cast<ShutdownRequest>(request);
        _builder.context()->logBook(shared_from_this());
        if (buildRequest != nullptr) {
            if (!_builder.running()) {
                _builder.completor().AddRaw(this, &BuildService::handleBuildCompletion);
                _builder.start(buildRequest);
            }
        } else if (stopRequest != nullptr || shutdownRequest != nullptr) {
            _shutdown = shutdownRequest != nullptr;
            if (_builder.running()) {
                _builder.stop();
            } else {
                send(std::make_shared<BuildResult>(true));
            }
        } else {
            if (_builder.running()) _builder.stop(); // unknown request type
        }
    }

    void BuildService::handleBuildCompletion(std::shared_ptr<BuildResult> result) {
        _builder.completor().RemoveObject(this);
        send(result);
    }

    void BuildService::send(std::shared_ptr<IStreamable> msg) {
        std::shared_ptr<TcpStream> client;
        std::shared_ptr<BuildServiceProtocol> protocol;
        {
            std::lock_guard<std::mutex> lock(_connectMutex);
            client = _client;
            protocol = _protocol;
        }
        if (client != nullptr && protocol != nullptr) {
            try {
                protocol->send(msg);
            } catch (std::exception&) {
            } catch (EndOfStreamException) {
            }
        }
    }
    void BuildService::closeClient() {
        std::lock_guard<std::mutex> lock(_connectMutex);
        if (_client != nullptr) {
            _client->closeSocket();
            _client = nullptr;
        }
        _protocol = nullptr;
    }
}