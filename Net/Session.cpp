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
#include "Session.h"

#include "../Gameplay/Stage.h"

#include <chrono>

#include "PacketError.h"

#include "../Configuration.h"
#include "../Console.h"
#include "../IO/UI.h"
#include "../IO/UITypes/UINotice.h"

namespace ms
{
	namespace
	{
		// Milliseconds on a clock that cannot jump backwards. Wall time can,
		// and a clock that goes backwards would either never time out or time
		// out at once.
		int64_t now_ms()
		{
			using namespace std::chrono;

			return duration_cast<milliseconds>(
				steady_clock::now().time_since_epoch()).count();
		}
	}

	Session::Session()
	{
		connected = false;
		length = 0;
		pos = 0;
	}

	Session::~Session()
	{
		if (connected)
			socket.close();
	}

	bool Session::init(const char* host, const char* port)
	{
		// Connect to the server.
		connected = socket.open(host, port);

		if (connected)
		{
			// Read keys necessary for communicating with the server.
			cryptography = { socket.get_buffer() };
		}

		return connected;
	}

	Error Session::init()
	{
		std::string HOST = Setting<ServerIP>::get().load();
		std::string PORT = Setting<ServerPort>::get().load();

		if (!init(HOST.c_str(), PORT.c_str()))
			return Error::CONNECTION;

		last_heard = now_ms();
		last_sent = now_ms();

		return Error::NONE;
	}

	bool Session::reconnect_to_configured()
	{
		if (connected)
			return true;

		std::string HOST = Setting<ServerIP>::get().load();
		std::string PORT = Setting<ServerPort>::get().load();

		return init(HOST.c_str(), PORT.c_str());
	}

	void Session::reconnect(const char* address, const char* port)
	{
	    printf("called reconnect\n");
		// Close the current connection and open a new one.
		bool success = socket.close();

		if (success)
			init(address, port);
		else
			connected = false;
	}

	void Session::process(const int8_t* bytes, size_t available)
	{
		if (pos == 0)
		{
			// Pos is 0, meaning this is the start of a new packet. Start by determining length.
			length = cryptography.check_length(bytes);
			// Reading the length means we processed the header. Move forward by the header length.
			bytes = bytes + HEADER_LENGTH;
			available -= HEADER_LENGTH;
		}

		// Determine how much we can write. Write data into the buffer.
		size_t towrite = length - pos;

		if (towrite > available)
			towrite = available;

		memcpy(buffer + pos, bytes, towrite);
		pos += towrite;

		// Check if the current packet has been fully processed.
		if (pos >= length)
		{
			cryptography.decrypt(buffer, length);

			try
			{
				packetswitch.forward(buffer, length);
			}
			catch (const PacketError& err)
			{
				Console::get().print(err.what());
			}

			pos = 0;
			length = 0;

			// Check if there is more available.
			size_t remaining = available - towrite;

			if (remaining >= MIN_PACKET_LENGTH)
			{
				// More packets are available, so we start over.
				process(bytes + towrite, remaining);
			}
		}
	}

	void Session::write(int8_t* packet_bytes, size_t packet_length)
	{
		if (!connected)
			return;

		int8_t header[HEADER_LENGTH];
		cryptography.create_header(header, packet_length);
		cryptography.encrypt(packet_bytes, packet_length);

		// dispatch reports failure and this used to discard it, so sends into a
		// dead socket looked successful. Report it the same way a failed read
		// does - silently dropping every outgoing packet from here on, with
		// the world still drawing, is far more confusing than saying so.
		if (!socket.dispatch(header, HEADER_LENGTH) ||
			!socket.dispatch(packet_bytes, packet_length))
		{
			disconnected();

			return;
		}

		last_sent = now_ms();
	}

	void Session::disconnected()
	{
		if (!connected)
			return;

		connected = false;

		printf("[!] connection to the server was lost\n");

		// Back to the login screen, NOT out of the game.
		//
		// Losing the host is an ordinary event here, not a fatal one: the
		// device hosting is a handheld somebody may close, drop, or carry out
		// of range. Quitting the whole game for it means the other players
		// have to start the app again to do anything - when what they want is
		// to pick a different host, or host themselves, both of which are two
		// taps away on the screen this returns to.
		UI::get().emplace<UIOk>(
			"Lost the connection to the game.\\nThe host may have closed it or gone out of range.",
			[](bool)
			{
				Stage::get().clear();
				UI::get().change_state(UI::State::LOGIN);
			});
	}

	void Session::read()
	{
		bool was_connected = connected;

		// Silence means the host is gone ONLY IF WE HAVE BEEN QUIET TOO.
		//
		// Killing an app does not always close its sockets cleanly, so the
		// read below can succeed forever against a peer that no longer
		// exists, and something has to notice.
		//
		// But the server does not send a heartbeat. Cosmic pings from
		// Client.checkIfIdle, which Netty calls on an IdleStateEvent - only
		// when the connection has gone QUIET. While somebody is playing, the
		// connection is never idle, so the server sends nothing unsolicited
		// and has nothing it owes us. Treating that as death disconnected
		// healthy sessions about two minutes into play, every time, and it
		// looked exactly like the handheld server being killed - which it was
		// not; the server was still listening and Termux had not been touched
		// for a day.
		//
		// So the test is both directions. If we have not spoken either, the
		// connection IS idle, the server's idle handler owes us a ping, and
		// its absence means nobody is there. If we are talking and hearing
		// nothing back, that is an ordinary quiet map, and a dead socket will
		// surface as a failed write instead.
		// Four missed pings, not one and a half.
		//
		// AFK is the case this has to survive, and it is the case with the
		// least margin: standing still, the client sends nothing, so the only
		// thing keeping either clock alive is the server's idle ping. Cosmic's
		// IdleStateHandler is set to 30 seconds, and the client pongs the
		// moment one arrives, so both clocks refresh every 30s - but a 45s
		// limit left only fifteen seconds of slack, and this handheld server
		// was measured going 79 SECONDS between packets while busy. An
		// afternoon of standing in town would have been cut off repeatedly.
		//
		// Being slow to notice a host that has genuinely gone is nearly free:
		// it costs a couple of minutes of a screen that is not responding
		// before the game says so. Cutting off a healthy session costs the
		// session. So this is deliberately generous.
		//
		// Nothing is lost by waiting, either - the SERVER hangs up on its own
		// if a client fails to pong within 15 seconds of a ping, so a client
		// that has really stopped answering is dealt with from that end.
		constexpr int64_t SILENCE_LIMIT = 120'000;

		if (connected && last_heard > 0 && last_sent > 0)
		{
			int64_t now = now_ms();

			if (now - last_heard > SILENCE_LIMIT && now - last_sent > SILENCE_LIMIT)
			{
				disconnected();

				return;
			}
		}

		// Check if a packet has arrived. Handle if data is sufficient: 4 bytes(header) + 2 bytes(opcode) = 6.
		size_t result = socket.receive(&connected);

		// Say so, once, when the link goes down. Without this the client keeps
		// drawing the world and sending input into nothing, which reads as the
		// game quietly breaking - attacks that never land, monsters that deal
		// no damage - rather than as a lost connection.
		if (was_connected && !connected)
		{
			// receive() has already cleared the flag; put it back so
			// disconnected() sees the transition and reports it once.
			connected = true;
			disconnected();
		}

		if (result >= MIN_PACKET_LENGTH || length > 0)
		{
			last_heard = now_ms();

			// Retrieve buffer from the socket and process it.
			const int8_t* bytes = socket.get_buffer();
			process(bytes, result);
		}
	}

	void Session::reconnect()
	{
		std::string HOST = Setting<ServerIP>::get().load();
		std::string PORT = Setting<ServerPort>::get().load();

		reconnect(HOST.c_str(), PORT.c_str());
	}

	bool Session::is_connected() const
	{
		return connected;
	}
}