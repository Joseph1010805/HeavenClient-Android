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
#include "BuddyList.h"

#include <algorithm>

namespace ms
{
	void BuddyList::update(std::vector<BuddyEntry>&& in)
	{
		entries = std::move(in);

		// Online first, then alphabetical. A list that reorders itself as
		// people log in and out is the whole point of having one.
		std::sort(entries.begin(), entries.end(),
			[](const BuddyEntry& a, const BuddyEntry& b)
			{
				if (a.online() != b.online())
					return a.online();

				return a.name < b.name;
			});
	}

	void BuddyList::clear()
	{
		entries.clear();
	}

	const std::vector<BuddyEntry>& BuddyList::get_entries() const
	{
		return entries;
	}

	size_t BuddyList::count() const
	{
		return entries.size();
	}

	size_t BuddyList::count_online() const
	{
		return std::count_if(entries.begin(), entries.end(),
			[](const BuddyEntry& e) { return e.online(); });
	}
}
