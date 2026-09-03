//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../OutPacket.h"

namespace ms
{
	// GIFTS, THE GAME'S OWN WAY.
	//
	// Duey is the parcel service: you hand over an item and a name, and the
	// other player collects it whenever they next log in. That is exactly the
	// gift this client wanted, and the server side of it has always been here
	// - Cosmic's DueyProcessor is complete and USE_DUEY is on. Nothing in this
	// client ever sent the packet.
	//
	// ⚠ IT STAYS INSIDE ONE WORLD, which is the point. Text may cross between
	// worlds through the post box; an ITEM must not, or the same sword exists
	// twice. Duey works through the server's own database and cannot leave it.
	//
	// Every operation is the same opcode with a leading byte saying which -
	// the codes are DueyProcessor.Actions, read out of the server rather than
	// guessed.
	namespace Duey
	{
		enum Action : uint8_t
		{
			// "Open the counter" - the server answers with everything waiting.
			OPEN = 0x00,
			SEND = 0x02,
			CLAIM = 0x04,
			DISCARD = 0x05,
			CLOSE = 0x07
		};
	}

	// Ask what is waiting. The reply is PARCEL with operation 8.
	// Opcode: DUEY_ACTION(65)
	class DueyOpenPacket : public OutPacket
	{
	public:
		DueyOpenPacket() : OutPacket(OutPacket::Opcode::DUEY_ACTION)
		{
			write_byte(Duey::Action::OPEN);
		}
	};

	// Hand something over.
	// Opcode: DUEY_ACTION(65)
	//
	// ⚠ THE SERVER CHARGES 5,000 MESOS on top of whatever is sent, and refuses
	// outright if the recipient is on the SAME ACCOUNT - so this cannot be
	// used to shuffle items between your own characters. Both are Cosmic's
	// rules, not this client's; see DueyProcessor.dueySendItem.
	//
	// `quick` is the paid express service and needs a Quick Delivery Ticket in
	// the cash bag. Sending true without one is treated as a packet edit and
	// DISCONNECTS the player, so it is not offered.
	class DueySendPacket : public OutPacket
	{
	public:
		DueySendPacket(int8_t inventory, int16_t slot, int16_t amount,
			int32_t mesos, const std::string& recipient)
			: OutPacket(OutPacket::Opcode::DUEY_ACTION)
		{
			write_byte(Duey::Action::SEND);
			write_byte(static_cast<uint8_t>(inventory));
			write_short(slot);
			write_short(amount);
			write_int(mesos);
			write_string(recipient);

			// Not quick, so no message follows. The server only reads one
			// when this byte is set - writing a string here anyway would
			// leave bytes unread and the handler would take them for the
			// next packet.
			write_byte(0);
		}
	};

	// Take one out of the counter and into the bag.
	// Opcode: DUEY_ACTION(65)
	class DueyClaimPacket : public OutPacket
	{
	public:
		DueyClaimPacket(int32_t parcel) : OutPacket(OutPacket::Opcode::DUEY_ACTION)
		{
			write_byte(Duey::Action::CLAIM);
			write_int(parcel);
		}
	};
}
