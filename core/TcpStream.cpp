#include "TcpStream.h"

namespace YAM
{
    TcpStream::TcpStream(boost::asio::ip::tcp::socket& socket)
        : _socket(socket)
        , _eos(false)
    {}

    void TcpStream::read(void* bytes, std::size_t nBytes)
    {
        boost::system::error_code error;
        char* b = static_cast<char*>(bytes);
        std::size_t nRead = 0;
        while (nRead != nBytes && !error.failed()) {
            _socket.wait(boost::asio::socket_base::wait_read);
            std::size_t len = _socket.read_some(boost::asio::buffer(b+nRead, nBytes-nRead), error);
            nRead += len;
        }
        _eos = error.failed();
        if (_eos) throw EndOfStreamException(error.message());
    }

    bool TcpStream::eos()
    {
        return _eos;
    }

    void TcpStream::write(const void* bytes, std::size_t nBytes)
    {
        boost::system::error_code error;
        const char* b = static_cast<const char*>(bytes);
        std::size_t nWritten = 0;
        while (nWritten != nBytes && !error) {
            _socket.wait(boost::asio::socket_base::wait_write);
            std::size_t len = _socket.write_some(boost::asio::buffer(b + nWritten, nBytes - nWritten), error);
            nWritten += len;
        }
        _eos = error.failed();
        if (_eos) throw EndOfStreamException(error.message());
    }

    void TcpStream::close() {
        _eos = true;
        _socket.close();
    }
}
