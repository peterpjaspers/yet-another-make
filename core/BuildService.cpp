#include "BuildService.h"
#include "TcpStream.h"
#include "BuildServiceProtocol.h"
#include "BuildServiceMessageTypes.h"

#include <string>

namespace YAM
{
    BuildService::BuildService(bool publishPort)
        : _service(boost::asio::ip::tcp::v4(), 0)
        , _acceptor(_context, _service)
        , _shutdown(false)
        , _thread(&BuildService::run, this)
    {
        if (publishPort) {
            auto servicePort = port();
            //TODO: save port in .yam/.servicePort directory
        }
    }

    boost::asio::ip::port_type BuildService::port() const {
        return _acceptor.local_endpoint().port();
    }

    void BuildService::join() {
        _thread.join();
    }

    void BuildService::run() {
        try {
            boost::asio::ip::tcp::socket socket(_context);
            while (!_shutdown) {
                _acceptor.accept(socket);
                _client = std::make_shared<TcpStream>(socket);
                _protocol = std::make_shared<BuildServiceProtocol>(_client, _client, false);
                std::shared_ptr<IStreamable> request = _protocol->receive();
                handleRequest(request);
            }
        } catch (std::exception& e) {
            _client->close();
            _client = nullptr;
            handleRequest(nullptr);
        }
    }

    void BuildService::add(LogRecord const& record) {
        auto ptr = std::make_shared<LogRecord>(record);
        _protocol->send(ptr);
    }

    void BuildService::handleRequest(std::shared_ptr<IStreamable> request) {
        auto buildRequest = dynamic_pointer_cast<BuildRequest>(request);
        auto stopRequest = dynamic_pointer_cast<StopBuildRequest>(request);
        auto shutdownRequest = dynamic_pointer_cast<ShutdownRequest>(request);
        if (buildRequest != nullptr) {
            if (!_builder.running()) {
                _builder.context()->logBook(shared_from_this());
                _builder.completor().AddRaw(this, &BuildService::handleBuildCompletion);
                _builder.start(buildRequest);
            }
        } else if (stopRequest != nullptr) {
            if (_builder.running()) _builder.stop();
        } else if (shutdownRequest != nullptr) {                
            if (_builder.running()) _builder.stop(); // protocol error
            _shutdown = true;
        } else {
            if (_builder.running()) _builder.stop(); // unknown request type
        }
    }

    void BuildService::handleBuildCompletion(std::shared_ptr<BuildResult> result) {
        _builder.completor().RemoveObject(this);
        if (_client != nullptr) {
            _protocol->send(result);
        }
    }
}