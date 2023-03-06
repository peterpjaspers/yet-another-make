#include "BuildService.h"
#include "TcpStream.h"
#include "BuildRequest.h"
#include "StopBuildRequest.h"
#include "ShutdownRequest.h"
#include "BuildResult.h"
#include "Streamer.h"
#include "BinaryValueStreamer.h"
#include "SharedObjectStreamer.h"
#include "BuildServiceMessageStreamer.h"

#include <iostream>
#include <string>
#include <boost/asio.hpp>

namespace
{
    using namespace YAM;

    std::shared_ptr<IStreamable> receiveRequest(TcpStream* stream) {
        std::shared_ptr<IStreamable> request;
        BinaryValueReader valueReader(stream);
        BuildServiceMessageReader msgReader;
        SharedObjectReader sharedMsgReader(&msgReader);
        Streamer reader(&valueReader, &sharedMsgReader);
        reader.stream(request);
        return request;
    }

    template<typename T>
    void sendReply(TcpStream* stream, std::shared_ptr<T> reply)
    {
        auto streamable = dynamic_pointer_cast<IStreamable>(reply);
        BinaryValueWriter valueWriter(stream);
        BuildServiceMessageWriter msgWriter;
        SharedObjectWriter sharedMsgWriter(&msgWriter);
        Streamer writer(&valueWriter, &sharedMsgWriter);
        writer.stream(streamable);
    }
}

namespace YAM
{
    BuildService::BuildService()
        : _shutdown(false)
        , _thread(&BuildService::run, this)
    {}

    void BuildService::run() {
        try {
            boost::asio::io_context context;
            boost::asio::ip::tcp::socket socket(context);
            boost::asio::ip::tcp::endpoint service(boost::asio::ip::tcp::v4(), 0);
            boost::asio::ip::tcp::acceptor acceptor(context, service);
            unsigned short servicePort = acceptor.local_endpoint().port();
            // TODO: publish port in .yam directory
            while (!_shutdown) {
                acceptor.accept(socket);
                _client = std::make_shared<TcpStream>(socket);
                std::shared_ptr<IStreamable> request = receiveRequest(_client.get());
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
        sendReply(_client.get(), ptr);
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
            sendReply(_client.get(), result);
        }
    }
}