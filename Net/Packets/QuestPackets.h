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

#include "../../Template/Point.h"

namespace ms
{
	// Take a quest, hand one in, or give one up.
	// Opcode: QUEST_ACTION(107)
	//
	// `byte action, short questid`, then whatever that action needs.
	//
	// Cosmic checks the NPC is actually near the player before doing anything
	// - `isNpcNearby` reads the two shorts on the end and refuses at more
	// than 1200 across or 800 down - so the position is sent even though it
	// looks redundant. Leave it out and every quest silently fails at the
	// distance check instead.
	class QuestActionPacket : public OutPacket
	{
	public:
		enum Action : int8_t
		{
			RESTORE_ITEM = 0,
			START = 1,
			COMPLETE = 2,
			FORFEIT = 3,
			// A quest whose Check.img carries `startscript` or `endscript`
			// runs an NPC conversation instead of a plain exchange. Sending
			// the plain action for one of these makes the server do nothing
			// at all, quietly.
			SCRIPTED_START = 4,
			SCRIPTED_END = 5
		};

		QuestActionPacket(Action action, int16_t questid, int32_t npcid, Point<int16_t> playerpos)
			: OutPacket(OutPacket::Opcode::QUEST_ACTION)
		{
			write_byte(action);
			write_short(questid);
			write_int(npcid);
			write_short(playerpos.x());
			write_short(playerpos.y());
		}

		// Giving up needs no NPC and no position.
		QuestActionPacket(int16_t questid)
			: OutPacket(OutPacket::Opcode::QUEST_ACTION)
		{
			write_byte(FORFEIT);
			write_short(questid);
		}
	};
}
