//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "SocketAsio.h"

#ifdef USE_ASIO
namespace ms
{
	SocketAsio::SocketAsio() : resolver(ioservice), socket(ioservice) {}

	SocketAsio::~SocketAsio()
	{
		if (socket.is_open())
		{
			error_code error;
			socket.close(error);
		}
	}

	bool SocketAsio::open(const char* address, const char* port)
	{
		tcp::resolver::query query(address, port);
		tcp::resolver::iterator endpointiter = resolver.resolve(query);
		error_code error;
		asio::connect(socket, endpointiter, error);

		if (!error)
		{
			size_t result = socket.read_some(asio::buffer(buffer), error);

			if (error || result != HANDSHAKE_LEN)
				return false;

			// Switch to non-blocking now the handshake is in. receive() can
			// then attempt a read every frame instead of only reading when
			// bytes are already waiting - and it is that "only when waiting"
			// guard which hid disconnections, because a dead link has nothing
			// waiting and so never reached the code that reports an error.
			socket.non_blocking(true, error);

			return !error;
		}

		return !error;
	}

	bool SocketAsio::close()
	{
		error_code error;
		socket.shutdown(tcp::socket::shutdown_both, error);
		socket.close(error);

		return !error;
	}

	size_t SocketAsio::receive(bool* recvok)
	{
		error_code error;
		size_t result = socket.read_some(asio::buffer(buffer), error);

		if (!error)
		{
			// A read of nothing on a readable socket is an orderly shutdown.
			if (result == 0)
				*recvok = false;

			return result;
		}

		// Nothing to read yet. This is the ordinary case on a non-blocking
		// socket and says nothing about the connection's health.
		if (error == asio::error::would_block || error == asio::error::try_again)
			return 0;

		// Anything else - eof, connection reset, a killed server - means the
		// link is gone. Reporting it is what lets the client stop pretending
		// to play: previously it went on sending into a dead socket, so a
		// dropped connection looked like combat that did nothing.
		*recvok = false;

		return 0;
	}

	const int8_t* SocketAsio::get_buffer() const
	{
		return buffer;
	}

	bool SocketAsio::dispatch(const int8_t* bytes, size_t length)
	{
		error_code error;
		size_t result = asio::write(socket, asio::buffer(bytes, length), error);

		return !error && (result == length);
	}
}
#endif