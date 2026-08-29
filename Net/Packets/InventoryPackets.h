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

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "../Character/Inventory/Inventory.h"

// UseItemPacket plays a sound in its constructor. This header never said so,
// and got away with it because every file that included it happened to include
// Audio.h first - until one did not, and the error named a line nobody had
// touched. Said out loud here so the next new file does not have to find out.
#include "../../Audio/Audio.h"

namespace ms
{
	// Packet which requests that the inventory is sorted.
	// Opcode: GATHER_ITEMS(69)
	class GatherItemsPacket : public OutPacket
	{
	public:
		GatherItemsPacket(InventoryType::Id type) : OutPacket(OutPacket::Opcode::GATHER_ITEMS)
		{
			write_time();
			write_byte(type);
		}
	};

	// Packet which requests that the inventory is sorted.
	// Opcode: SORT_ITEMS(70)
	class SortItemsPacket : public OutPacket
	{
	public:
		SortItemsPacket(InventoryType::Id type) : OutPacket(OutPacket::Opcode::SORT_ITEMS)
		{
			write_time();
			write_byte(type);
		}
	};

	// Packet which requests that an item is moved.
	// Opcode: MOVE_ITEM(71)
	class MoveItemPacket : public OutPacket
	{
	public:
		MoveItemPacket(InventoryType::Id type, int16_t slot, int16_t action, int16_t qty) : OutPacket(OutPacket::Opcode::MOVE_ITEM)
		{
			// The server reads this packet by the SIGNS of slot and action -
			// `action < 0` equips, `action == 0` DROPS, otherwise it moves -
			// so a wrong number here is the difference between wearing an
			// item and throwing it on the floor. Worth a line each time while
			// that is still being pinned down.
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_INFO, "HeavenClient",
				"MOVE_ITEM tab=%d slot=%d action=%d qty=%d -> %s",
				(int)type, (int)slot, (int)action, (int)qty,
				action < 0 ? "EQUIP" : (action == 0 ? "DROP" : "MOVE"));
#endif

			write_time();
			write_byte(type);
			write_short(slot);
			write_short(action);
			write_short(qty);
		}
	};

	// Packet which requests that an item is equipped.
	// Opcode: MOVE_ITEM(71)
	class EquipItemPacket : public MoveItemPacket
	{
	public:
		EquipItemPacket(int16_t src, Equipslot::Id dest) : MoveItemPacket(InventoryType::Id::EQUIP, src, -dest, 1) {}
	};

	// Packet which requests that an item is unequipped.
	// Opcode: MOVE_ITEM(71)
	class UnequipItemPacket : public MoveItemPacket
	{
	public:
		UnequipItemPacket(int16_t src, int16_t dest) : MoveItemPacket(InventoryType::Id::EQUIPPED, -src, dest, 1) {}
	};

	// A packet which requests that an 'USE' item is used.
	// Opcode: USE_ITEM(72)
	class UseItemPacket : public OutPacket
	{
	public:
		UseItemPacket(int16_t slot, int32_t itemid) : OutPacket(OutPacket::Opcode::USE_ITEM)
		{
			Sound(itemid).play();

			write_time();
			write_short(slot);
			write_int(itemid);
		}
	};

	// Use an item out of the CASH tab.
	// Opcode: USE_CASH_ITEM(79)
	//
	// No timestamp on this one, unlike USE_ITEM - Cosmic's handler reads the
	// slot first. It rate-limits to one cash item every three seconds and
	// says so in chat when you are early.
	//
	// Note what is NOT here: rate coupons. A 2x EXP or drop coupon
	// (5211xxx / 5360xxx) is never used at all - the server applies it just
	// for sitting in your cash inventory during the hours it is scheduled
	// for, in the `nxcoupons` table. Tapping one does nothing on purpose.
	class UseCashItemPacket : public OutPacket
	{
	public:
		UseCashItemPacket(int16_t slot, int32_t itemid) : OutPacket(OutPacket::Opcode::USE_CASH_ITEM)
		{
			write_short(slot);
			write_int(itemid);
		}
	};

	// The megaphone BUTTON.
	// Opcode: USE_CASH_ITEM(79)
	//
	// The same opcode and the same leading fields as UseCashItemPacket, because
	// it is the same request. UseCashItemHandler reads the slot and the item
	// id, works out the kind from (id / 1000) % 10, and then reads whatever
	// that kind needs:
	//
	//   5070000  channel   message
	//   5071000  world     message, then a byte for the "ear"
	//
	// Slot 0 because nothing is being taken out of the inventory. The server
	// looks that slot up, finds nothing, and carries on anyway when
	// USE_FREE_MEGAPHONES is set - which is the whole trick. Everything the
	// player sees is then produced by the megaphone code that has always been
	// there, so a button and an item cannot look different.
	//
	// Deliberately NOT a new opcode. Cosmic decides what some packets mean
	// partly by their LENGTH, and a wrong guess there fails silently - it
	// disconnects, or does nothing, and says neither.
	class MegaphonePacket : public OutPacket
	{
	public:
		MegaphonePacket(int32_t itemid, const std::string& message, bool ear)
			: OutPacket(OutPacket::Opcode::USE_CASH_ITEM)
		{
			write_short(0);
			write_int(itemid);
			write_string(message);

			// Only the super megaphone carries it, and reading a byte that was
			// never written is how a handler runs off the end of a packet.
			if (((itemid / 1000) % 10) == 2)
				write_byte(ear ? 1 : 0);
		}
	};

	// Requests using a scroll on an equip.
	// Opcode: SCROLL_EQUIP(86)
	class ScrollEquipPacket : public OutPacket
	{
	public:
		enum Flag : uint8_t
		{
			NONE = 0x00,
			UNKNOWN = 0x01,
			WHITESCROLL = 0x02
		};

		ScrollEquipPacket(int16_t source, Equipslot::Id target, uint8_t flags) : OutPacket(OutPacket::Opcode::SCROLL_EQUIP)
		{
			write_time();
			write_short(source);
			write_short(-target);
			write_short(flags);
		}

		ScrollEquipPacket(int16_t source, Equipslot::Id target) : ScrollEquipPacket(source, target, 0) {}
	};
}