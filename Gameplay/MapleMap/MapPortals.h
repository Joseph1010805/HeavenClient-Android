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

#include "Portal.h"

#include "../Template/Optional.h"

#include <unordered_map>

#include <nlnx/node.hpp>

namespace ms
{
	// Collection of portals on a map. Draws and updates portals.
	// Also contains methods for using portals and obtaining spawn points.
	class MapPortals
	{
	public:
		static void init();

		MapPortals(nl::node source, int32_t mapid);
		MapPortals();

		void update(Point<int16_t> playerpos);
		void draw(Point<int16_t> viewpos, float inter) const;

		Portal::WarpInfo find_warp_at(Point<int16_t> playerpos);

		// A portal that fires by being WALKED INTO rather than by pressing up.
		//
		// Types 3 and 9 are "collision" and "collisionScript" - contact, not a
		// keypress. The scripts behind them say the same thing: tutorMinimap
		// shows a hint and blocks itself, aranTutorLost plays an intro cutscene
		// and blocks itself. Nobody presses up to trigger a cutscene, and
		// blockPortal() only makes sense for something that would otherwise fire
		// again on its own.
		//
		// Returns the portal once per entry: standing on it does not re-fire, or
		// the client would send a packet several times a second for as long as
		// somebody stood still.
		Portal::WarpInfo find_touch_at(Point<int16_t> playerpos);

		Point<int16_t> get_portal_by_id(uint8_t id) const;
		Point<int16_t> get_portal_by_name(const std::string& name) const;

	private:
		static std::unordered_map<Portal::Type, Animation> animations;

		std::unordered_map<uint8_t, Portal> portals_by_id;
		std::unordered_map<std::string, uint8_t> portal_ids_by_name;

		static const int16_t WARPCD = 48;
		int16_t cooldown;

		// Which touch portal the player is standing in, so it fires on entry
		// and not on every frame afterwards. Empty when standing in none.
		std::string touching;
	};
}