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
	// EVERYTHING THAT HAPPENS IN A ROOM WITH ANOTHER PLAYER.
	//
	// One opcode, PLAYER_INTERACTION (0x13A coming in), carrying trades,
	// player shops, hired merchants and two minigames, told apart by its
	// first byte. This handles the trade branches and says plainly in the log
	// which of the others arrived, rather than swallowing them - an unhandled
	// branch that is silent is indistinguishable from a server that never
	// sent anything.
	class PlayerInteractionHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;
	};
}
