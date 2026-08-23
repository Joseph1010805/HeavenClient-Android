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
#include "Questlog.h"

namespace ms
{
	namespace
	{
		const std::string nothing;
	}

	void Questlog::add_started(int16_t qid, const std::string& qdata)
	{
		started[qid] = qdata;
	}

	void Questlog::add_in_progress(int16_t qid, int16_t qidl, const std::string& qdata)
	{
		in_progress[qid] = make_pair(qidl, qdata);
	}

	void Questlog::add_completed(int16_t qid, int64_t time)
	{
		completed[qid] = time;
	}

	void Questlog::set_started(int16_t qid, const std::string& progress)
	{
		completed.erase(qid);
		started[qid] = progress;
	}

	void Questlog::set_completed(int16_t qid, int64_t time)
	{
		started.erase(qid);
		in_progress.erase(qid);
		completed[qid] = time;
	}

	void Questlog::forget(int16_t qid)
	{
		started.erase(qid);
		in_progress.erase(qid);
	}

	bool Questlog::is_started(int16_t qid) const
	{
		return started.count(qid) > 0;
	}

	bool Questlog::is_completed(int16_t qid) const
	{
		return completed.count(qid) > 0;
	}

	int16_t Questlog::get_last_started() const
	{
		if (started.empty())
			return 0;

		return started.rbegin()->first;
	}

	const std::string& Questlog::get_progress(int16_t qid) const
	{
		auto iter = started.find(qid);

		return (iter == started.end()) ? nothing : iter->second;
	}

	int16_t Questlog::killed(int16_t qid, size_t which) const
	{
		// Three digits per monster, in the order the quest lists them.
		const std::string& progress = get_progress(qid);
		size_t at = which * 3;

		if (progress.size() < at + 3)
			return 0;

		const std::string count = progress.substr(at, 3);

		for (char c : count)
			if (c < '0' || c > '9')
				return 0;

		return static_cast<int16_t>(std::stoi(count));
	}

	std::vector<int16_t> Questlog::all_started() const
	{
		std::vector<int16_t> out;

		for (const auto& entry : started)
			out.push_back(entry.first);

		return out;
	}

	std::vector<int16_t> Questlog::all_completed() const
	{
		std::vector<int16_t> out;

		for (const auto& entry : completed)
			out.push_back(entry.first);

		return out;
	}

	Questlog::State Questlog::state_of(int16_t qid) const
	{
		if (is_completed(qid))
			return State::COMPLETED;

		if (is_started(qid))
			return State::STARTED;

		// Whether it is AVAILABLE cannot be answered here - it depends on the
		// character's level, job and inventory. Player::quest_state() decides
		// that, and this is the half of the answer the log itself holds.
		return State::UNAVAILABLE;
	}
}
