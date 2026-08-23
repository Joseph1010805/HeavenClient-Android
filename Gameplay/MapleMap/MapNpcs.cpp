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
#include "MapNpcs.h"
#include "Npc.h"

#include "../Data/QuestData.h"
#include "../Gameplay/Stage.h"
#include "../Net/Packets/NpcInteractionPackets.h"
#include "../Net/Packets/QuestPackets.h"

namespace ms
{
	void MapNpcs::draw(Layer::Id layer, double viewx, double viewy, float alpha) const
	{
		npcs.draw(layer, viewx, viewy, alpha);
	}

	void MapNpcs::update(const Physics& physics)
	{
		for (; !spawns.empty(); spawns.pop())
		{
			const NpcSpawn& spawn = spawns.front();

			int32_t oid = spawn.get_oid();
			Optional<MapObject> npc = npcs.get(oid);

			if (npc)
				npc->makeactive();
			else
				npcs.add(spawn.instantiate(physics));
		}

		npcs.update(physics);

		refresh_quest_marks();
	}

	// Who has something to offer.
	//
	// Recomputed every few seconds rather than every frame - it walks the
	// quests attached to each NPC and checks level, job, prerequisites and
	// inventory against each - and after anything that could change the
	// answer, which is most of what a player does.
	void MapNpcs::refresh_quest_marks()
	{
		if (--until_refresh > 0)
			return;

		until_refresh = REFRESH_TICKS;

		const Player& player = Stage::get().get_player();

		for (auto& map_object : npcs)
		{
			Npc* npc = static_cast<Npc*>(map_object.second.get());

			if (!npc || !npc->is_active())
				continue;

			int32_t npcid = npc->get_npcid();

			// Handing one in beats taking one: a player standing in front of
			// an NPC who can finish their quest wants to be told that, not
			// offered a new one.
			if (player.quest_to_finish(npcid))
				npc->set_quest_mark(Npc::QuestMark::COMPLETABLE);
			else if (player.quest_to_start(npcid))
				npc->set_quest_mark(Npc::QuestMark::AVAILABLE);
			else
				npc->set_quest_mark(Npc::QuestMark::NONE);
		}
	}

	void MapNpcs::spawn(NpcSpawn&& spawn)
	{
		spawns.emplace(std::move(spawn));
	}

	void MapNpcs::remove(int32_t oid)
	{
		if (auto npc = npcs.get(oid))
			npc->deactivate();
	}

	void MapNpcs::clear()
	{
		npcs.clear();
	}

	MapObjects * MapNpcs::get_npcs()
	{
		return &npcs;
	}

	// Talking to an NPC is not one thing.
	//
	// A quest is started or handed in by the CLIENT asking for it by number,
	// and only then; the ordinary conversation packet does not carry a quest
	// and the server will not volunteer one. So the quest comes first when
	// there is one, and a plain chat otherwise.
	void MapNpcs::talk_to(Npc& npc)
	{
		const Player& player = Stage::get().get_player();

		int32_t npcid = npc.get_npcid();
		Point<int16_t> at = player.get_position();

		if (int16_t finishing = player.quest_to_finish(npcid))
		{
			const QuestData& data = QuestData::get(finishing);

			QuestActionPacket(data.to_finish().scripted
				? QuestActionPacket::SCRIPTED_END
				: QuestActionPacket::COMPLETE,
				finishing, npcid, at).dispatch();

			return;
		}

		if (int16_t starting = player.quest_to_start(npcid))
		{
			const QuestData& data = QuestData::get(starting);

			QuestActionPacket(data.to_start().scripted
				? QuestActionPacket::SCRIPTED_START
				: QuestActionPacket::START,
				starting, npcid, at).dispatch();

			return;
		}

		TalkToNPCPacket(npc.get_oid()).dispatch();
	}

	Cursor::State MapNpcs::send_cursor(bool pressed, Point<int16_t> position, Point<int16_t> viewpos)
	{
		for (auto& map_object : npcs)
		{
			Npc* npc = static_cast<Npc*>(map_object.second.get());

			if (npc && npc->is_active() && npc->inrange(position, viewpos))
			{
				if (pressed)
				{
					talk_to(*npc);

					return Cursor::State::IDLE;
				}
				else
				{
					return Cursor::State::CANCLICK;
				}
			}
		}

		return Cursor::State::IDLE;
	}
}