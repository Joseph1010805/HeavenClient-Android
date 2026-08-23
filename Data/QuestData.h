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

#include "../Template/Cache.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ms
{
	// Everything Quest.nx knows about one quest.
	//
	// WHY THIS HAS TO EXIST AT ALL, since it is not obvious: in this version
	// the CLIENT owns quest availability. It is the client that knows which
	// quests an NPC offers, whether the player meets the requirements, and
	// which of them is worth an exclamation mark over the NPC's head - and it
	// is the client that starts one, by sending QUEST_ACTION. The server only
	// validates what it is asked for; Cosmic's QuestScriptManager is reached
	// from nothing except that packet.
	//
	// This client had never opened Quest.nx. The file was loaded at startup
	// and copied to the device, and not one line read it - which is why no
	// NPC has ever offered a quest, and why the character in the database has
	// two auto-started tutorial flags and nothing else after weeks of play.
	//
	// Four archives make up a quest:
	//
	//   QuestInfo.img  its name, its area, and three pieces of prose - what
	//                  to do, the journal entry while doing it, and the one
	//                  after finishing
	//   Check.img      what is required, in two phases: [0] to start it,
	//                  [1] to hand it in
	//   Act.img        what is given, in the same two phases
	//   Say.img        what the NPC says (not read here yet)
	class QuestData
	{
	public:
		// What a phase asks for. Empty fields simply do not apply - most
		// quests use two or three of these.
		struct Requirements
		{
			// The NPC to talk to. Every phase of nearly every quest has one.
			int32_t npc = 0;

			int16_t lvmin = 0;
			int16_t lvmax = 0;

			// Job ids that may take it. Empty means anyone.
			std::vector<int16_t> jobs;

			// Other quests and the state they must be in - 2 is finished.
			std::map<int16_t, int8_t> quests;

			// Items to be holding, and monsters to have killed.
			std::map<int32_t, int16_t> items;
			std::map<int32_t, int16_t> mobs;

			// Whether this phase runs an NPC script rather than a plain
			// exchange. It decides which action byte QUEST_ACTION carries:
			// scripted is 4 and 5, plain is 1 and 2. Getting it wrong means
			// the server quietly does nothing.
			bool scripted = false;
		};

		// What a phase hands over.
		struct Rewards
		{
			int32_t exp = 0;
			int32_t money = 0;
			int16_t fame = 0;

			// Positive counts are given, negative are taken away.
			std::map<int32_t, int16_t> items;

			int16_t nextquest = 0;
		};

		static const QuestData& get(int16_t questid);

		bool is_valid() const;

		int16_t get_id() const;
		const std::string& get_name() const;
		// The three journal strings: 0 what to do, 1 while doing it, 2 after.
		const std::string& get_text(size_t which) const;
		int32_t get_area() const;

		const Requirements& to_start() const;
		const Requirements& to_finish() const;
		const Rewards& start_rewards() const;
		const Rewards& finish_rewards() const;

		// Every quest that names this NPC in the phase given. Built once, on
		// first use, by walking all 2,800 quests - the alternative is walking
		// them every time an NPC comes into view.
		static const std::vector<int16_t>& quests_of_npc(int32_t npcid, bool finishing);

	private:
		friend Cache<QuestData>;

		QuestData(int16_t questid);

		int16_t questid;
		bool valid;

		std::string name;
		std::string text[3];
		int32_t area;

		Requirements start;
		Requirements finish;
		Rewards startgives;
		Rewards finishgives;
	};
}
