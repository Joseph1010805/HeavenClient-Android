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

#include "../Template/Point.h"

#include <cstdint>
#include <map>

namespace ms
{
	// WHERE THE QUEST WANTS YOU TO GO.
	//
	// One quest at a time, chosen with the NAVIGATE button on its page in the
	// journal. Everything that can point at something asks this: the world
	// marker bobbing over the NPC's head, the ring round them on the minimap,
	// and the arrow over your own character when they are off to one side.
	//
	// A SINGLETON RATHER THAN STATE ON THE JOURNAL. The journal is a window
	// that is usually shut - on the lower panel it is not even built until
	// somebody opens that page - and the things that draw the marker are the
	// map and the minimap, which have no business reaching into a UI window
	// to ask what it is showing.
	//
	// It holds an NPC ID, not a position. NPCs walk.
	class QuestTracker
	{
	public:
		static QuestTracker& get();

		// Follow this quest. Passing the quest already being followed turns
		// tracking OFF, so the same button both starts and stops it.
		void track(int16_t quest_id, int32_t npc_id);
		void clear();

		bool tracking() const { return quest != 0; }
		int16_t get_quest() const { return quest; }
		int32_t get_npc() const { return npc; }

		// Whether this is the NPC being pointed at. Cheap, and safe to ask
		// once per NPC per frame.
		bool is_target(int32_t npc_id) const
		{
			return quest != 0 && npc != 0 && npc_id == npc;
		}

		// The tracked NPC's position on THIS map, and whether it was found.
		//
		// False means the NPC is somewhere else - which is a thing worth
		// saying out loud rather than drawing an arrow that points nowhere.
		bool find_target(Point<int16_t>& out) const;

		// WHICH MAP THE TRACKED NPC LIVES ON, or 0 if we cannot tell.
		//
		// Nothing in the quest data says where an NPC stands - Quest.nx names
		// the person, and only Map.nx knows where people are. So the first
		// time anyone asks, every map's `life` list is read once and an index
		// of npc -> map is built. It is a few hundred milliseconds and it
		// happens on a button press, not at startup, because most sessions
		// never need it.
		int32_t target_map() const;

	private:
		void build_index() const;

		QuestTracker() = default;

		int16_t quest = 0;
		int32_t npc = 0;

		// npc id -> the first map it was found on. Built on demand; a map
		// with the same NPC twice keeps the first, which is what "where do I
		// find him" means anyway.
		mutable std::map<int32_t, int32_t> npc_map;
		mutable bool indexed = false;
	};
}
