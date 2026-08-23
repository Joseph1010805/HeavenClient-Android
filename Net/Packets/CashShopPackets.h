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
	// Opcode: BUY_CS_ITEM(229) = Cosmic's CASHSHOP_OPERATION. Buy is
	// action 0x03: byte action, byte pad, int currency (1 = NX credit,
	// 2 = maple points, 4 = prepaid), int commodity SN.
	class BuyCashItemPacket : public OutPacket
	{
	public:
		// Request the server to purchase a cash shop item
		BuyCashItemPacket(int8_t currency, int32_t sn_item_id) : OutPacket(OutPacket::Opcode::BUY_CS_ITEM)
		{
			write_byte(3);
			write_byte(0);
			write_int(currency);
			write_int(sn_item_id);
		}
	};

	// Move an item out of the cash shop's locker and onto the character.
	// Action 0x0D, and the id is the item's cash id, truncated to an int -
	// the server reads it with readInt().
	class TakeFromCashInventoryPacket : public OutPacket
	{
	public:
		TakeFromCashInventoryPacket(int64_t cashid) : OutPacket(OutPacket::Opcode::BUY_CS_ITEM)
		{
			write_byte(0x0D);
			write_int(static_cast<int32_t>(cashid));
		}
	};

	// Leave the cash shop and go back to the world.
	//
	// Cosmic's ChangeMapHandler decides what a CHANGE_MAP means by its
	// LENGTH: `enteringMapFromCashShop = p.available() == 0`. An empty body
	// is the way out of the shop; a body of any size, sent while the shop is
	// open, is read as a hack and the client is disconnected outright -
	// which is what the white screen on exit was. So this packet carries
	// nothing, deliberately, and nothing may be added to it.
	//
	// The reply is CHANGE_CHANNEL: leaving the shop is a reconnect to the
	// channel server, not a map change. See ChangeChannelHandler.
	class ExitCashShopPacket : public OutPacket
	{
	public:
		ExitCashShopPacket() : OutPacket(OutPacket::Opcode::CHANGEMAP) {}
	};

	// Opcode: COUPON_CODE(216)
	class CouponCodePacket : public OutPacket
	{
	public:
		// Redeem a coupon code in the cash shop
		CouponCodePacket(const std::string& code) : OutPacket(OutPacket::Opcode::COUPON_CODE)
		{
			write_string(code);
		}
	};
}
