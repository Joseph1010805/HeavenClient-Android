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
#include "ItemParser.h"

namespace ms
{
	namespace ItemParser
	{
		// Parse a normal item from a packet.
		void add_item(InPacket& recv, InventoryType::Id invtype, int16_t slot, int32_t id, Inventory& inventory)
		{
			// Read all item stats.
			bool cash = recv.read_bool();

			if (cash)
				recv.skip(8); // unique id

			int64_t expire = recv.read_long();
			int16_t count = recv.read_short();
			std::string owner = recv.read_string();
			int16_t flag = recv.read_short();

			// If the item is a rechargeable projectile, some additional bytes are sent.
			if ((id / 10000 == 233) || (id / 10000 == 207))
				recv.skip(8);

			inventory.add_item(invtype, slot, id, cash, expire, count, owner, flag);
		}

		// Parse a pet from a packet.
		void add_pet(InPacket& recv, InventoryType::Id invtype, int16_t slot, int32_t id, Inventory& inventory)
		{
			// Read all pet stats.
			bool cash = recv.read_bool();

			if (cash)
				recv.skip(8); // unique id

			int64_t expire = recv.read_long();
			std::string petname = recv.read_padded_string(13);
			int8_t petlevel = recv.read_byte();
			int16_t closeness = recv.read_short();
			int8_t fullness = recv.read_byte();

			// Some unused bytes.
			recv.skip(18);

			inventory.add_pet(invtype, slot, id, cash, expire, petname, petlevel, closeness, fullness);
		}

		// Parse an equip from a packet.
		void add_equip(InPacket& recv, InventoryType::Id invtype, int16_t slot, int32_t id, Inventory& inventory)
		{
			// Read equip information.
			bool cash = recv.read_bool();

			if (cash)
				recv.skip(8); // unique id

			int64_t expire = recv.read_long();
			uint8_t slots = recv.read_byte();
			uint8_t level = recv.read_byte();

			// Read equip stats.
			EnumMap<Equipstat::Id, uint16_t> stats;

			for (auto iter : stats)
				iter.second = recv.read_short();

			// Some more information.
			std::string owner = recv.read_string();
			int16_t flag = recv.read_short();
			uint8_t itemlevel = 0;
			uint16_t itemexp = 0;
			int32_t vicious = 0;

			if (cash)
			{
				// Some unused bytes.
				recv.skip(10);
			}
			else
			{
				recv.read_byte();
				itemlevel = recv.read_byte();
				recv.read_short();
				itemexp = recv.read_short();
				vicious = recv.read_int();
				recv.read_long();
			}

			recv.skip(12);

			if (slot < 0)
			{
				invtype = InventoryType::Id::EQUIPPED;
				slot = -slot;
			}

			inventory.add_equip(invtype, slot, id, cash, expire, slots, level, stats, owner, flag, itemlevel, itemexp, vicious);
		}

		// Read an item WITHOUT putting it anywhere. See ItemParser.h.
		//
		// The three shapes below mirror add_item, add_pet and add_equip byte
		// for byte. If one of those changes, this changes with it - and the
		// symptom of forgetting will be a trade window that reads the
		// partner's item and then loses the rest of the packet, because the
		// stream is left in the wrong place.
		Skimmed skim_item(InPacket& recv)
		{
			Skimmed out;

			recv.read_byte(); // 'type' byte
			out.id = recv.read_int();

			bool cash = recv.read_bool();

			if (cash)
				recv.skip(8); // unique id

			recv.read_long(); // expiry

			if (out.id >= 1000000 && out.id < 2000000)
			{
				// An equip. One of a kind, so the count is always one.
				out.count = 1;

				recv.read_byte(); // upgrade slots
				recv.read_byte(); // upgrade level

				for (size_t i = 0; i < Equipstat::Id::LENGTH; i++)
					recv.read_short();

				recv.read_string(); // owner
				recv.read_short();  // flag

				if (cash)
				{
					recv.skip(10);
				}
				else
				{
					recv.read_byte();
					recv.read_byte();  // item level
					recv.read_short();
					recv.read_short(); // item exp
					recv.read_int();   // vicious
					recv.read_long();
				}

				recv.skip(12);

				return out;
			}

			if (out.id >= 5000000 && out.id <= 5000102)
			{
				// A pet. Not tradeable - the server refuses them - but the
				// stream still has to be walked correctly if one ever
				// arrives.
				out.count = 1;

				recv.read_padded_string(13);
				recv.read_byte();
				recv.read_short();
				recv.read_byte();
				recv.skip(18);

				return out;
			}

			out.count = recv.read_short();

			recv.read_string(); // owner
			recv.read_short();  // flag

			if ((out.id / 10000 == 233) || (out.id / 10000 == 207))
				recv.skip(8);

			return out;
		}

		void parse_item(InPacket& recv, InventoryType::Id invtype, int16_t slot, Inventory& inventory)
		{
			// Read type and item id.
			recv.read_byte(); // 'type' byte
			int32_t iid = recv.read_int();

			if (invtype == InventoryType::Id::EQUIP || invtype == InventoryType::Id::EQUIPPED)
			{
				// Parse an equip.
				add_equip(recv, invtype, slot, iid, inventory);
			}
			else if (iid >= 5000000 && iid <= 5000102)
			{
				// Parse a pet.
				add_pet(recv, invtype, slot, iid, inventory);
			}
			else
			{
				// Parse a normal item.
				add_item(recv, invtype, slot, iid, inventory);
			}
		}
	}
}