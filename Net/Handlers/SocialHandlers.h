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

namespace ms
{
	// Every party message the server sends, from an invitation arriving to
	// somebody being expelled. The first byte picks which.
	// Opcode: PARTY_OPERATION(62)
	class PartyOperationHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;
	};

	// The friends list, and every change to it.
	//
	// This was a NullHandler, so the list the server sends on every login has
	// been arriving and being discarded since the client was written.
	//
	// Opcode: BUDDY_LIST(63)
	class BuddyListHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// A party member's health changed.
	//
	// Sent whenever somebody in the party takes damage or heals, and while
	// they are on the same map as you - which is the only time it is of any
	// use, since it feeds the small health bars drawn over their heads.
	//
	// Party::update_member_hp and PartyHpBar were both already written and
	// waiting; nothing ever called them, because this message had no handler
	// at all. The bars existed and stayed empty.
	//
	// Opcode: UPDATE_PARTYMEMBER_HP(201)
	class UpdatePartyMemberHpHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;
	};
}
