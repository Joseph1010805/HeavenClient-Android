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

#include "../Template/Enumeration.h"
#include "../Template/EnumMap.h"

#include <cstdint>

namespace ms
{
	namespace Equipslot
	{
		enum Id : int16_t
		{
			NONE = 0,
			HAT = 1,
			FACE = 2,
			EYEACC = 3,
			EARACC = 4,
			TOP = 5,
			BOTTOM = 6,
			SHOES = 7,
			GLOVES = 8,
			CAPE = 9,
			SHIELD = 10, // TODO: Where is this now?
			WEAPON = 11,
			RING1 = 12,
			RING2 = 13,
			RING3 = 15,
			RING4 = 16,
			PENDANT1 = 17,
			TAMEDMOB = 18, // TODO: Where is this now?
			SADDLE = 19, // TODO: Where is this now?
			MEDAL = 49,
			BELT = 50,
			POCKET, // TODO: What is the proper value for this?
			BOOK, // TODO: What is the proper value for this?
			PENDANT2, // TODO: What is the proper value for this?
			SHOULDER, // TODO: What is the proper value for this?
			// Named ANDROID upstream, after the in-game android companion.
			// The Android NDK defines ANDROID as a preprocessor macro, which
			// turns this identifier into a literal and breaks the enum. Renamed
			// rather than #undef'd, because an #undef here would depend on
			// include order within every translation unit.
			ANDROID_SLOT, // TODO: What is the proper value for this?
			EMBLEM, // TODO: What is the proper value for this?
			BADGE, // TODO: What is the proper value for this?
			SUBWEAPON, // TODO: What is the proper value for this?
			HEART, // TODO: What is the proper value for this?
			LENGTH
		};

		Id by_id(size_t id);

		// Cash equips are worn in a SECOND set of slots, 100 above the real
		// ones - a cash hat goes to 101, not 1.
		//
		// They are purely cosmetic. The cash item decides what the character
		// looks like; the ordinary item underneath keeps its stats and stays
		// on. Sending a cash equip to the ordinary slot knocks the real gear
		// off and takes its stats with it, which is what "the weapon
		// discards when you try to equip it" was.
		//
		// The server checks this too: `EquipSlot.isAllowed` compares the
		// destination against `allowed - 100` for a cash item, and answers a
		// non-cash item aimed at a cash slot with an autoban warning. So the
		// offset must be applied for cash items and ONLY for cash items.
		constexpr int16_t CASH_OFFSET = 100;

		// The cash counterpart of an ordinary slot, and back again.
		constexpr Id cash_of(Id slot)
		{
			return static_cast<Id>(static_cast<int16_t>(slot) + CASH_OFFSET);
		}

		constexpr bool is_cash_slot(int16_t slot)
		{
			return slot > CASH_OFFSET;
		}

		constexpr Id base_of(int16_t slot)
		{
			return static_cast<Id>(is_cash_slot(slot) ? slot - CASH_OFFSET : slot);
		}

		constexpr Enumeration<Id> values;
	};
}