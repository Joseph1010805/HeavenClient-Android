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
#include "QuestTracker.h"

#include "Stage.h"
#include "MapleMap/Npc.h"

#include <nlnx/nx.hpp>
#include <string>
#include <cstdio>

namespace ms
{
	QuestTracker& QuestTracker::get()
	{
		static QuestTracker instance;

		return instance;
	}

	void QuestTracker::track(int16_t quest_id, int32_t npc_id)
	{
		// The same quest again means "stop pointing" - one button, both ways,
		// because a separate STOP button would be on screen doing nothing for
		// all the time nothing is being tracked.
		if (quest == quest_id)
		{
			clear();

			return;
		}

		quest = quest_id;
		npc = npc_id;
	}

	void QuestTracker::clear()
	{
		quest = 0;
		npc = 0;
	}

	bool QuestTracker::find_target(Point<int16_t>& out) const
	{
		if (!tracking() || npc == 0 || !Stage::get().is_active())
			return false;

		MapObjects* npcs = Stage::get().get_npcs().get_npcs();

		if (!npcs)
			return false;

		for (auto iter = npcs->begin(); iter != npcs->end(); ++iter)
		{
			Npc* who = static_cast<Npc*>(iter->second.get());

			if (who && who->get_npcid() == npc)
			{
				out = who->get_position();

				return true;
			}
		}

		// Tracked, but not on this map - say so once rather than every frame.
		static int32_t moaned_about = 0;

		if (moaned_about != npc)
		{
			moaned_about = npc;
			printf("[ ] navigate: npc %d is not on this map\n",
				static_cast<int>(npc));
		}

		return false;
	}

	void QuestTracker::build_index() const
	{
		if (indexed)
			return;

		indexed = true;

		// EVERY MAP'S LIFE LIST, ONCE.
		//
		// Map.nx is Map/Map0..Map9/<9 digits>.img, and each map's `life` node
		// holds its NPCs and its monsters mixed together, told apart by a
		// `type` of "n" or "m". Only names and small integers are read - no
		// bitmap is touched - so this is a walk over the node table rather
		// than anything that decompresses.
		nl::node maps = nl::nx::map["Map"];

		for (nl::node group : maps)
		{
			for (nl::node one : group)
			{
				std::string name = one.name();

				// "<id>.img" -> id. Anything else in here is not a map.
				if (name.size() < 5 || name.substr(name.size() - 4) != ".img")
					continue;

				int32_t map_id = 0;

				try
				{
					map_id = std::stoi(name.substr(0, name.size() - 4));
				}
				catch (...)
				{
					continue;
				}

				for (nl::node life : one["life"])
				{
					if (std::string(life["type"]) != "n")
						continue;

					// THE ID IS A STRING, NOT A NUMBER.
					//
					// Map.nx stores it as text - "9000036", not 9000036 - and
					// nlnx answers an integer conversion on a string node with
					// ZERO. So every NPC in the game was filed under id 0 and
					// the index, which looked healthy at 1,349 entries, could
					// not find a single one of them.
					//
					// `type` is a string too, which is why THAT comparison was
					// already written this way. The two fields sit next to
					// each other; only one of them was read correctly.
					int32_t npc_id = 0;

					try
					{
						npc_id = std::stoi(std::string(life["id"]));
					}
					catch (...)
					{
						continue;
					}

					// FIRST ONE WINS. Several maps hold the same NPC - a
					// shopkeeper with a twin in another town - and "where do I
					// find him" wants an answer, not a list.
					npc_map.emplace(npc_id, map_id);
				}
			}
		}
	}

	int32_t QuestTracker::target_map() const
	{
		if (!tracking() || npc == 0)
			return 0;

		build_index();

		auto found = npc_map.find(npc);

		return found == npc_map.end() ? 0 : found->second;
	}
}