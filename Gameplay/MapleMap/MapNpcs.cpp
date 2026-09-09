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

#include "../../Util/Silent.h"

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
		// ONCE, HERE - not once per NPC inside Npc::update. The two balloons
		// are one shared pair for the whole map, so stepping them per NPC ran
		// them as many times faster as there were quest NPCs on screen.
		Npc::update_markers();

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

		// The pending queue as well as the live objects.
		//
		// update() drains `spawns` into `npcs`, and a map change stops the
		// update loop (the graphics are locked and the timer restarted) while
		// the network thread keeps queueing. Anything that arrived during the
		// changeover therefore outlived the wipe and was instantiated into the
		// NEXT map - which is how four Tutorial Tinos from the Cygnus tutorial
		// ended up on Maple Road after a character change.
		std::queue<NpcSpawn>().swap(spawns);
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

		// WHAT THIS CLIENT DECIDED TO SAY, and why.
		//
		// Talking is three different packets and the choice is made HERE,
		// from the client's own reading of Check.img. When it guesses a quest
		// the server will not honour, the NPC does something other than talk
		// - so "he did nothing" and "he had nothing to say" and "I asked him
		// for the wrong thing" are three different faults that look the same.
		Silent::report("MapNpcs::talk_to",
			"npc " + std::to_string(npcid)
			+ " finish=" + std::to_string(player.quest_to_finish(npcid))
			+ " start=" + std::to_string(player.quest_to_start(npcid)));

		// ⚠ WE DO NOT GUESS THE QUEST ANY MORE. THE SERVER DECIDES.
		//
		// This used to work out which quest the NPC was offering - from the
		// client's own reading of Check.img - and send QUEST_ACTION instead
		// of a conversation, returning either way. That is one guess made
		// from SIX of the twenty requirements the server actually checks, and
		// when it was wrong the NPC did nothing whatsoever: no talk, no shop,
		// no script. One bad guess made an NPC permanently mute, because the
		// same guess was made on every press.
		//
		// It is also why a storage keeper could go silent - a wrong quest
		// guess meant his script never ran, so the bank never opened.
		//
		// OPENSTORY DOES NOT DO THIS. It sends a plain talk and lets the
		// server sort it out, which is why it needs no requirement model at
		// all. Our server already works that way too: QuestDialogue.tryTalk
		// picks the quest, runs its dialogue, and falls through to the shop
		// or the NPC's own script - it was written for exactly this.
		//
		// So the guess is deleted rather than improved. The quest BALLOONS
		// still use the client's reading, but an optimistic balloon is
		// cosmetic where a wrong packet was fatal.
		TalkToNPCPacket(npc.get_oid()).dispatch();
	}

	Cursor::State MapNpcs::send_cursor(bool pressed, Point<int16_t> position, Point<int16_t> viewpos)
	{
		// ⚠ A PRESS THAT HITS NOTHING SAYS SO, WITH THE NUMBERS.
		//
		// Clicking Empress Cygnus did nothing at all, and talk_to's own report
		// never fired - so the press was not being ignored, it was never
		// matching an NPC in the first place. That leaves two candidates and
		// no way to tell them apart from the sofa: the press is not reaching
		// this function, or it is and every hit box misses.
		//
		// inrange() sizes its box from animations.at(stance), so an NPC whose
		// current stance has no entry gets a ZERO-SIZED box and becomes
		// permanently unclickable while still drawing perfectly. That is
		// invisible from the outside and exactly what this prints.
		if (pressed)
		{
			std::string seen;

			for (auto& map_object : npcs)
			{
				Npc* npc = static_cast<Npc*>(map_object.second.get());

				if (!npc)
					continue;

				Point<int16_t> at = npc->get_position() + viewpos;

				seen += " [" + std::to_string(npc->get_npcid())
					+ " at " + std::to_string(at.x())
					+ "," + std::to_string(at.y())
					+ (npc->is_active() ? "" : " INACTIVE")
					+ (npc->inrange(position, viewpos) ? " HIT" : "")
					+ "]";
			}

			Silent::report("MapNpcs",
				"press at " + std::to_string(position.x())
				+ "," + std::to_string(position.y())
				+ " - npcs:" + (seen.empty() ? " none" : seen));
		}

		// ⚠ THE NEAREST ONE, NOT THE FIRST ONE.
		//
		// Click boxes OVERLAP, and taking whichever the map happened to store
		// first is arbitrary. On Ereve, Shinsoo's box is 242 wide and reaches
		// 680; Cygnus stands at 681 with a box starting at 618, so the two
		// share a 62-pixel strip. A tap in that strip went to Shinsoo purely
		// because she is stored first - and Cygnus, the one with the quest
		// balloon over her head, could not be reached at all.
		//
		// Nearest CENTRE wins. It matches what the player meant - you aim at
		// a character, not at a rectangle - and it settles overlaps without
		// needing to know which is drawn on top.
		Npc* best = nullptr;
		int32_t best_distance = 0;

		for (auto& map_object : npcs)
		{
			Npc* npc = static_cast<Npc*>(map_object.second.get());

			if (!npc || !npc->is_active() || !npc->inrange(position, viewpos))
				continue;

			Point<int16_t> centre = npc->get_click_centre() + viewpos;

			int32_t dx = centre.x() - position.x();
			int32_t dy = centre.y() - position.y();
			int32_t distance = dx * dx + dy * dy;

			if (!best || distance < best_distance)
			{
				best = npc;
				best_distance = distance;
			}
		}

		if (!best)
			return Cursor::State::IDLE;

		if (pressed)
		{
			talk_to(*best);

			return Cursor::State::IDLE;
		}

		return Cursor::State::CANCLICK;
	}
}