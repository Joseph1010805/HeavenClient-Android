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

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ms
{
	// What this character has done, is doing, and could do about quests.
	//
	// It used to be three maps and a way to put things in them - nothing ever
	// asked it a question, because nothing in the client could act on the
	// answer. Now that the client is the side that decides what an NPC has to
	// offer, this is where that decision is made.
	class Questlog
	{
	public:
		enum class State : uint8_t
		{
			// Never taken, and the requirements are not met.
			UNAVAILABLE,
			// Never taken, and it could be taken now.
			AVAILABLE,
			// Taken and not finished.
			STARTED,
			// Taken, finished, and cannot be taken again.
			COMPLETED
		};

		void add_started(int16_t, const std::string& quest_data);
		void add_in_progress(int16_t, int16_t, const std::string& quest_data);
		void add_completed(int16_t, int64_t);

		// Called when the server says a quest changed state, so the log does
		// not have to wait for the next character load to agree with it.
		void set_started(int16_t qid, const std::string& progress);
		void set_completed(int16_t qid, int64_t time);
		void forget(int16_t qid);

		bool is_started(int16_t) const;
		bool is_completed(int16_t) const;
		int16_t get_last_started() const;

		// The progress string the server keeps for a started quest. For a
		// kill-count quest it is a run of three-digit numbers, one per
		// monster, in the order Check.img lists them - "005003" is five of
		// the first and three of the second.
		const std::string& get_progress(int16_t qid) const;

		// How far along a kill requirement is. `which` is the position of the
		// monster in the quest's own list.
		int16_t killed(int16_t qid, size_t which) const;

		// Every started quest, for the log window and the tracker.
		std::vector<int16_t> all_started() const;
		std::vector<int16_t> all_completed() const;

		// Whether the requirements to take this quest are met right now.
		// Needs the character, so it lives on Player rather than here - this
		// only reports what the log itself knows.
		State state_of(int16_t qid) const;

	private:
		std::map<int16_t, std::string> started;
		std::map<int16_t, std::pair<int16_t, std::string>> in_progress;
		std::map<int16_t, int64_t> completed;
	};
}
