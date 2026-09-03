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
#pragma once

#include "../OutPacket.h"

namespace ms
{
	// Packet which sends a message to general chat.
	// Opcode: GENERAL_CHAT(49)
	// PULL A FACE, and let everyone in the map see it.
	//
	// The client could already DRAW an expression on its own character - that
	// is what CharLook::set_expression does - but never told the server, so
	// nobody else ever saw one. Both halves are needed: the packet so the
	// others see it, and the local set so it happens instantly rather than
	// after a round trip.
	class FaceExpressionPacket : public OutPacket
	{
	public:
		FaceExpressionPacket(int32_t expression) : OutPacket(OutPacket::Opcode::FACE_EXPRESSION)
		{
			write_int(expression);
		}
	};

	class GeneralChatPacket : public OutPacket
	{
	public:
		GeneralChatPacket(const std::string& message, bool show) : OutPacket(OutPacket::Opcode::GENERAL_CHAT)
		{
			write_string(message);
			write_byte(show);
		}
	};

	// Sends a private message to one player by name.
	//
	// The first byte is a flag pair from Cosmic's WhisperFlag: WHISPER(0x02)
	// says what kind of request this is and REQUEST(0x04) says it is a request
	// rather than a reply, so 0x06 together. The server answers on the same
	// opcode with RESULT(0x08) - success or "no such player" - and delivers to
	// the target with RECEIVE(0x10).
	//
	// Opcode: WHISPER(120)
	class WhisperPacket : public OutPacket
	{
	public:
		WhisperPacket(const std::string& target, const std::string& message) : OutPacket(OutPacket::Opcode::WHISPER)
		{
			write_byte(0x02 | 0x04);
			write_string(target);
			write_string(message);
		}
	};

	// Asks which channel/map a player is on. Same opcode, LOCATION(0x01) in
	// place of WHISPER.
	class FindPlayerPacket : public OutPacket
	{
	public:
		FindPlayerPacket(const std::string& target) : OutPacket(OutPacket::Opcode::WHISPER)
		{
			write_byte(0x01 | 0x04);
			write_string(target);
		}
	};
}