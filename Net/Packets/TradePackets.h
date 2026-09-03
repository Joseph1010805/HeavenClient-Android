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

#include "../../Character/Inventory/InventoryType.h"

namespace ms
{
	// TRADING, WHICH IS ONE OPCODE WEARING TWENTY HATS.
	//
	// Everything two players do to each other in a room - trades, player
	// shops, hired merchants, omok, match cards - goes out on
	// PLAYER_INTERACTION (0x7B) and is told apart by the first byte. Cosmic
	// calls that byte the Action and lists the whole set in
	// PlayerInteractionHandler.java; only the ones a trade uses are here.
	//
	// The names below are Cosmic's, deliberately, so that a packet in this
	// file and the branch that receives it can be found by the same word.
	// They read oddly on their own - VISIT means "accept the invitation",
	// because in the original the invitation is to visit a room - and
	// renaming them would only mean translating twice.
	namespace TradeAction
	{
		enum Id : int8_t
		{
			CREATE = 0x00,
			INVITE = 0x02,
			DECLINE = 0x03,
			VISIT = 0x04,
			ROOM = 0x05,
			CHAT = 0x06,
			CHAT_THING = 0x08,
			EXIT = 0x0A,
			SET_ITEMS = 0x0F,
			SET_MESO = 0x10,
			CONFIRM = 0x11
		};
	}

	// STEP ONE: MAKE A TRADE TO INVITE SOMEBODY INTO.
	//
	// The invitation cannot be sent on its own - the server looks up
	// chr.getTrade() and finds nothing. This has to go first, every time.
	class TradeStartPacket : public OutPacket
	{
	public:
		TradeStartPacket() : OutPacket(OutPacket::Opcode::PLAYER_INTERACTION)
		{
			write_byte(TradeAction::CREATE);

			// 3 is a trade. 1 is omok, 2 is match cards, 4 and 5 are shops.
			write_byte(3);
		}
	};

	// STEP TWO: ASK SOMEBODY.
	//
	// By character id, and they must be on this map - the server looks them
	// up with chr.getMap().getCharacterById and gives up quietly if they are
	// not there.
	class TradeInvitePacket : public OutPacket
	{
	public:
		TradeInvitePacket(int32_t character_id) : OutPacket(OutPacket::Opcode::PLAYER_INTERACTION)
		{
			write_byte(TradeAction::INVITE);
			write_int(character_id);
		}
	};

	// SAYING YES. No payload: the server already knows which trade this
	// answers, because being invited is what put one on the character.
	class TradeAcceptPacket : public OutPacket
	{
	public:
		TradeAcceptPacket() : OutPacket(OutPacket::Opcode::PLAYER_INTERACTION)
		{
			write_byte(TradeAction::VISIT);
		}
	};

	// SAYING NO.
	class TradeDeclinePacket : public OutPacket
	{
	public:
		TradeDeclinePacket() : OutPacket(OutPacket::Opcode::PLAYER_INTERACTION)
		{
			write_byte(TradeAction::DECLINE);
		}
	};

	// WALKING OUT. Also what cancels a trade that has already started - the
	// server sorts out which by whether a trade is open.
	class TradeExitPacket : public OutPacket
	{
	public:
		TradeExitPacket() : OutPacket(OutPacket::Opcode::PLAYER_INTERACTION)
		{
			write_byte(TradeAction::EXIT);
		}
	};

	// PUTTING SOMETHING ON THE TABLE.
	//
	// The trade slot is 1 to 9 and the server REFUSES anything outside that
	// as a dupe attempt - it logs the character's name as a hacker. Count it
	// from one.
	class TradeSetItemPacket : public OutPacket
	{
	public:
		TradeSetItemPacket(InventoryType::Id type, int16_t source_slot,
			int16_t quantity, int8_t trade_slot)
			: OutPacket(OutPacket::Opcode::PLAYER_INTERACTION)
		{
			write_byte(TradeAction::SET_ITEMS);

			// The enum's own value IS the wire value - EQUIP 1, USE 2, SETUP
			// 3, ETC 4, CASH 5 - and Cosmic's InventoryType.getByType reads
			// it back with the same numbers.
			write_byte(static_cast<int8_t>(type));
			write_short(source_slot);
			write_short(quantity);
			write_byte(trade_slot);
		}
	};

	// PUTTING MONEY ON THE TABLE. The whole amount every time, not a
	// difference - the server takes it as the new total.
	class TradeSetMesoPacket : public OutPacket
	{
	public:
		TradeSetMesoPacket(int32_t meso) : OutPacket(OutPacket::Opcode::PLAYER_INTERACTION)
		{
			write_byte(TradeAction::SET_MESO);
			write_int(meso);
		}
	};

	// LOCKING IT IN.
	//
	// Not the end of the trade - it is one half of a handshake. The partner
	// is told (CONFIRM comes back to THEM, not to us), and the exchange only
	// happens when both sides have sent this. There is no going back
	// afterwards: the server refuses to change items or meso once locked.
	class TradeConfirmPacket : public OutPacket
	{
	public:
		TradeConfirmPacket() : OutPacket(OutPacket::Opcode::PLAYER_INTERACTION)
		{
			write_byte(TradeAction::CONFIRM);
		}
	};

	// TALKING ACROSS THE TABLE.
	class TradeChatPacket : public OutPacket
	{
	public:
		TradeChatPacket(const std::string& message) : OutPacket(OutPacket::Opcode::PLAYER_INTERACTION)
		{
			write_byte(TradeAction::CHAT);
			write_string(message);
		}
	};
}
