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

namespace ms
{
	// Class that represents the monster card collection of an individual character.
	class Monsterbook
	{
	public:
		Monsterbook();

		void set_cover(int32_t);
		void add_card(int16_t, int8_t);

		// WHAT IS IN IT. The book has been WRITE-ONLY since it was ported -
		// login filled it in and nothing could ever read it back, which is
		// why there has never been a page to look at.
		//
		// Keyed by the card's own id, which is the monster id with the first
		// digit dropped; the value is how many of that card you hold, 1 to 5.
		const std::map<int16_t, int8_t>& get_cards() const { return cards; }

		int32_t get_cover() const { return cover; }

		// How many DIFFERENT monsters have been recorded, which is the number
		// worth showing - a second copy of a card you already have is not
		// progress.
		size_t size() const { return cards.size(); }

	private:
		int32_t cover;
		std::map<int16_t, int8_t> cards;
	};
}