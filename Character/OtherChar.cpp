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
#include "OtherChar.h"

#include "../Constants.h"

namespace ms
{
	OtherChar::OtherChar(int32_t id, const CharLook& lk, uint8_t lvl, int16_t jb, const std::string& nm, int8_t st, Point<int16_t> pos) : Char(id, lk, nm)
	{
		level = lvl;
		job = jb;
		set_position(pos);

		lastmove.xpos = pos.x();
		lastmove.ypos = pos.y();
		lastmove.newstate = st;

		attackspeed = 6;
		attacking = false;
	}

	int8_t OtherChar::update(const Physics& physics)
	{
		if (!attacking)
			set_state(lastmove.newstate);

		// Ease toward the last position the server reported, closing a share
		// of the remaining gap each tick.
		//
		// This replaces a queue that played movements back one per tick behind
		// a fixed delay. That model had to choose between lag and stutter: too
		// short a delay and a burst of packets snapped, too long and everyone
		// else was visibly behind - it was 400ms, which was most of the second
		// of lag between two devices here. Following the newest position
		// instead needs no such choice. Nothing queues, so nothing can fall
		// behind and stay behind.
		double dx = lastmove.xpos - phobj.crnt_x();
		double dy = lastmove.ypos - phobj.crnt_y();

		phobj.hspeed = dx * FOLLOW_FACTOR;
		phobj.vspeed = dy * FOLLOW_FACTOR;
		phobj.move();

		physics.get_fht().update_fh(phobj);

		bool aniend = Char::update(physics, get_stancespeed());

		if (aniend && attacking)
			attacking = false;

		return get_layer();
	}

	void OtherChar::send_movement(const std::vector<Movement>& newmoves)
	{
		if (newmoves.empty())
			return;

		lastmove = newmoves.back();

		// Easing is right for walking and wrong for everything else. A
		// teleport, a flash jump, or a character that drifted out of sync
		// would be slid across the map at walking pace, arriving somewhere
		// they left long ago - so past a certain distance, go straight there.
		double dx = lastmove.xpos - phobj.crnt_x();
		double dy = lastmove.ypos - phobj.crnt_y();

		if (dx * dx + dy * dy > SNAP_DISTANCE * SNAP_DISTANCE)
		{
			phobj.set_x(lastmove.xpos);
			phobj.set_y(lastmove.ypos);
			phobj.hspeed = 0.0;
			phobj.vspeed = 0.0;
		}
	}

	void OtherChar::update_skill(int32_t skillid, uint8_t skilllevel)
	{
		skilllevels[skillid] = skilllevel;
	}

	void OtherChar::update_speed(uint8_t as)
	{
		attackspeed = as;
	}

	void OtherChar::update_look(const LookEntry& newlook)
	{
		look = newlook;

		uint8_t laststate = lastmove.newstate;
		set_state(laststate);
	}

	int8_t OtherChar::get_integer_attackspeed() const
	{
		return attackspeed;
	}

	uint16_t OtherChar::get_level() const
	{
		return level;
	}

	int32_t OtherChar::get_skilllevel(int32_t skillid) const
	{
		auto iter = skilllevels.find(skillid);

		if (iter == skilllevels.end())
			return 0;

		return iter->second;
	}
}