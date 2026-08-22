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

#include "../Character/Maplestat.h"

#include "../IO/UITypes/UIKeyConfig.h"

namespace ms
{
	// Requests a stat increase by spending ap.
	// Opcode: SPEND_AP(87)
	// Tells the server what was recovered by standing still.
	// Opcode 89, read by Cosmic's HealOvertimeHandler as: eight bytes skipped,
	// then HP, then MP.
	//
	// The server watches this: two heals less than 1.5 seconds apart are
	// treated as cheating, and HP above 77 * the map's recovery rate * 1.5 is
	// an instant ban. A tick of ten every ten seconds is far inside both.
	class HealOverTimePacket : public OutPacket
	{
	public:
		HealOverTimePacket(int16_t hp, int16_t mp) : OutPacket(OutPacket::Opcode::HEAL_OVER_TIME)
		{
			skip(8);

			write_short(hp);
			write_short(mp);
		}
	};

	class SpendApPacket : public OutPacket
	{
	public:
		SpendApPacket(Maplestat::Id stat) : OutPacket(OutPacket::Opcode::SPEND_AP)
		{
			write_time();
			write_int(Maplestat::codes[stat]);
		}
	};

	// Requests a skill level increase by spending sp.
	// Opcode: SPEND_SP(90)
	class SpendSpPacket : public OutPacket
	{
	public:
		SpendSpPacket(int32_t skill_id) : OutPacket(OutPacket::Opcode::SPEND_SP)
		{
			write_time();
			write_int(skill_id);
		}
	};

	// Requests the server to change kep mappings
	// Opcode: CHANGE_KEYMAP(135)
	class ChangeKeyMapPacket : public OutPacket
	{
	public:
		ChangeKeyMapPacket(std::vector<std::tuple<KeyConfig::Key, KeyType::Id, int32_t>> updated_actions) : OutPacket(OutPacket::Opcode::CHANGE_KEYMAP)
		{
			write_int(0); // mode
			write_int(updated_actions.size()); // Number of key changes

			for (size_t i = 0; i < updated_actions.size(); i++)
			{
				auto keymap = updated_actions[i];

				write_int(std::get<0>(keymap)); // key
				write_byte(std::get<1>(keymap));// type
				write_int(std::get<2>(keymap)); // action
			}
		}
	};
}