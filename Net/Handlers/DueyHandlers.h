//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../PacketHandler.h"

namespace ms
{
	// EVERYTHING DUEY EVER SAYS, on one opcode.
	//
	// Operation 8 is the counter itself - every parcel waiting, sent whole.
	// Everything else is a one-byte answer to something that was just asked:
	// sent, collected, no such name, bag full.
	//
	// Opcode: PARCEL(322)
	class ParcelHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;
	};
}
