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
	// THE BANK, which is per ACCOUNT and not per character.
	//
	// Which is the whole reason it is worth building here: it is how one of
	// this account's characters hands something to another without a second
	// device and a trade. Everything a character puts in, every character on
	// the account can take out.
	//
	// The window is OPENED by an NPC, not by us - a storage keeper's script
	// calls sendStorage and the packet simply arrives. There is no "open the
	// bank" request to send, and looking for one is a good way to waste an
	// afternoon.
	//
	// Cosmic refuses the lot below level 15 and says so in a chat line.
	namespace StorageAction
	{
		enum Id : int8_t
		{
			TAKE_OUT = 4,
			STORE = 5,
			ARRANGE = 6,
			MESO = 7,
			CLOSE = 8
		};
	}

	// TAKING SOMETHING OUT.
	//
	// The slot is the item's index WITHIN ITS OWN TAB, counted from zero -
	// the server does `storage.getSlot(type, slot)` to turn the pair back
	// into a position. Storage arrives as one flat list and the tabs are the
	// client's own doing, so this number only means anything alongside the
	// type it came with.
	class StorageTakeOutPacket : public OutPacket
	{
	public:
		StorageTakeOutPacket(InventoryType::Id type, int8_t slot)
			: OutPacket(OutPacket::Opcode::STORAGE)
		{
			write_byte(StorageAction::TAKE_OUT);
			write_byte(static_cast<int8_t>(type));
			write_byte(slot);
		}
	};

	// PUTTING SOMETHING IN. The bag slot here is the real one, counted from
	// ONE - the server rejects anything below it as a packet edit and
	// disconnects. Storage counts from zero and the bag counts from one; they
	// are not the same number and never were.
	class StorageStorePacket : public OutPacket
	{
	public:
		StorageStorePacket(int16_t bag_slot, int32_t item_id, int16_t quantity)
			: OutPacket(OutPacket::Opcode::STORAGE)
		{
			write_byte(StorageAction::STORE);
			write_short(bag_slot);
			write_int(item_id);
			write_short(quantity);
		}
	};

	// MONEY, IN ONE DIRECTION OR THE OTHER.
	//
	// POSITIVE takes meso OUT of storage and gives it to the character;
	// NEGATIVE puts it in. It reads backwards until you notice the server
	// does `storage.setMeso(storageMesos - meso)`.
	class StorageMesoPacket : public OutPacket
	{
	public:
		StorageMesoPacket(int32_t meso) : OutPacket(OutPacket::Opcode::STORAGE)
		{
			write_byte(StorageAction::MESO);
			write_int(meso);
		}
	};

	// Tidy up. Cosmic only obeys this with USE_STORAGE_ITEM_SORT on.
	class StorageArrangePacket : public OutPacket
	{
	public:
		StorageArrangePacket() : OutPacket(OutPacket::Opcode::STORAGE)
		{
			write_byte(StorageAction::ARRANGE);
		}
	};

	// Shut it. Worth sending: the server holds the storage open on the
	// character until it hears this.
	class StorageClosePacket : public OutPacket
	{
	public:
		StorageClosePacket() : OutPacket(OutPacket::Opcode::STORAGE)
		{
			write_byte(StorageAction::CLOSE);
		}
	};
}
