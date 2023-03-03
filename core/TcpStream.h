#pragma once

#include "IIOStream.h"
#include <boost/asio.hpp>

namespace YAM
{
	class __declspec(dllexport) TcpStream : public IInputStream, public IOutputStream
	{
	public:
		TcpStream(boost::asio::ip::tcp::socket& socket);

		// Inherited via IInputStream
		void read(void* bytes, std::size_t nBytes) override;
		bool eos() override;

		// Inherited via IOutputStream
		void write(const void* bytes, std::size_t nBytes) override;

		// close the tcp socket
		void close();

	private:
		boost::asio::ip::tcp::socket& _socket;
		bool _eos;
	};
}
