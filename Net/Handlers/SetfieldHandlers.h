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

#include "../PacketHandler.h"

#include "../Character/Player.h"

namespace ms
{
	// Reading a character out of a packet.
	//
	// These were private to SetfieldHandler, which is where a character
	// normally arrives - but it is not the only place. Entering and leaving
	// the cash shop re-sends the whole character too, and that handler needs
	// to read exactly the same bytes in exactly the same order. Sharing the
	// one implementation is the only way the two cannot drift apart.
	namespace CharacterParser
	{
		void parse_inventory(InPacket& recv, Inventory& inventory);
		void parse_skillbook(InPacket& recv, Skillbook& skills);
		void parse_cooldowns(InPacket& recv, Player& player);
		void parse_questlog(InPacket& recv, Questlog& quests);
		void parse_ring1(InPacket& recv);
		void parse_ring2(InPacket& recv);
		void parse_ring3(InPacket& recv);
		void parse_minigame(InPacket& recv);
		void parse_monsterbook(InPacket& recv, Monsterbook& monsterbook);
		void parse_telerock(InPacket& recv, Telerock& telerock);
		void parse_nyinfo(InPacket& recv);
		void parse_areainfo(InPacket& recv);
	}

	// Handler for a packet which contains all character information on first login
	// or warps the player to a different map.
	class SetfieldHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;

	private:
		void transition(int32_t mapid, uint8_t portalid) const;
		void change_map(InPacket& recv, int32_t map_id) const;
		void set_field(InPacket& recv) const;

	};
}