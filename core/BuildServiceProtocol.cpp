#include "BuildServiceProtocol.h"
#include "TcpStream.h"
#include "BuildServiceMessageTypes.h"
#include "Streamer.h"
#include "BinaryValueStreamer.h"
#include "ObjectStreamer.h"
#include "SharedObjectStreamer.h"

#include <string>

namespace
{
    using namespace YAM;

    BuildServiceMessageTypes types; // to initialize type ids of message classes

    // Service writes (sends) reply to client
    class MessageToClientWriter : public ObjectWriter
    {
    protected:
        uint32_t getTypeId(IStreamable* object) const override {
            uint32_t tid = object->typeId();
            auto mtid = static_cast<BuildServiceMessageTypes::MessageType>(tid);
            switch (mtid) {
                case BuildServiceMessageTypes::BuildResult: return tid;
                case BuildServiceMessageTypes::LogRecord: return tid;
                default: throw std::exception("Build service error: attempt to send illegal msg to client");
            }
            return tid;
        }
    };

    // Service reads (receives) request from client
    class MessageFromClientReader : public ObjectReader
    {
    protected:
        IStreamable* readObject(IStreamer* streamer, uint32_t typeId) const override {
            auto mtid = static_cast<BuildServiceMessageTypes::MessageType>(typeId);
            switch (mtid) {
                case BuildServiceMessageTypes::BuildRequest: return new BuildRequest(streamer);
                case BuildServiceMessageTypes::StopBuildRequest: return new StopBuildRequest(streamer);
                case BuildServiceMessageTypes::ShutdownRequest: return new ShutdownRequest(streamer);
                default: throw std::exception("Build service protocol error: illegal msg received by service");
            }
        }
    };

    // Writes (sends) request to build service
    class MessageToServiceWriter : public ObjectWriter
    {
    protected:
        uint32_t getTypeId(IStreamable* object) const override {
            uint32_t tid = object->typeId();
            auto mtid = static_cast<BuildServiceMessageTypes::MessageType>(tid);
            switch (mtid) {
                case BuildServiceMessageTypes::BuildRequest: return tid;
                case BuildServiceMessageTypes::StopBuildRequest: return tid;
                case BuildServiceMessageTypes::ShutdownRequest: return tid;
                default: throw std::exception("Build service protocol error: attempt to send illegal msg to service");
            }
        }
    };

    // Reads (receive) reply from build service
    class MessageFromServiceReader : public ObjectReader
    {
    protected:
        IStreamable* readObject(IStreamer* streamer, uint32_t typeId) const override {
            auto mtid = static_cast<BuildServiceMessageTypes::MessageType>(typeId);
            switch (mtid) {
                case BuildServiceMessageTypes::BuildResult: return new BuildResult(streamer);
                case BuildServiceMessageTypes::LogRecord: return new LogRecord(streamer);
                default: throw std::exception("Build service protocol error: illegal msg received by client");
            }
        }
    };
}

namespace YAM
{
    BuildServiceProtocol::BuildServiceProtocol(
            std::shared_ptr<IInputStream> istream,
            std::shared_ptr<IOutputStream> ostream,
            bool client)
        : _istream(istream)
        , _ostream(ostream)
    {
        if (client) {
            _msgDeserializer = std::make_shared<MessageFromServiceReader>();
            _msgSerializer = std::make_shared<MessageToServiceWriter>();
        } else {
            _msgDeserializer = std::make_shared<MessageFromClientReader>();
            _msgSerializer = std::make_shared<MessageToClientWriter>();
        }
    }

    std::shared_ptr<IStreamable> BuildServiceProtocol::receive() {
        std::shared_ptr<IStreamable> message;
        BinaryValueReader valueReader(_istream.get());
        SharedObjectReader sharedMsgReader(_msgDeserializer.get());
        Streamer reader(&valueReader, &sharedMsgReader);
        reader.stream(message);
        return message;
    }

    void BuildServiceProtocol::send(std::shared_ptr<IStreamable> message)
    {
        BinaryValueWriter valueWriter(_ostream.get());
        MessageToClientWriter msgWriter;
        SharedObjectWriter sharedMsgWriter(_msgSerializer.get());
        Streamer writer(&valueWriter, &sharedMsgWriter);
        writer.stream(message);
    }
}
