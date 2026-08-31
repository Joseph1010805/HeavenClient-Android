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
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ms
{
	// Who is on your friends list, and whether they are online.
	//
	// The server has been sending this on every login since the client was
	// written; opcode BUDDY_LIST was mapped to NullHandler, so it arrived and
	// was dropped every single time.
	class BuddyEntry
	{
	public:
		int32_t cid = 0;
		std::string name;
		std::string group;

		// Cosmic writes channel - 1, so anything below zero means the friend
		// is not logged in. There is no separate online flag.
		int32_t channel = -1;

		bool online() const { return channel >= 0; }
	};

	class BuddyList
	{
	public:
		void update(std::vector<BuddyEntry>&& entries);
		void clear();

		const std::vector<BuddyEntry>& get_entries() const;

		size_t count() const;
		size_t count_online() const;

	private:
		std::vector<BuddyEntry> entries;
	};
}
