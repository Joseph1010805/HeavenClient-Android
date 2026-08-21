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

namespace ms
{
	// Interface for tooltips, information windows about something
	// the mouse cursor is pointed at.
	class Tooltip
	{
	public:
		// Possible parent UIs for Tooltips.
		enum Parent
		{
			NONE,
			EQUIPINVENTORY,
			ITEMINVENTORY,
			SKILLBOOK,
			SHOP,
			EVENT,
			TEXT,
			KEYCONFIG,
			WORLDMAP,
			MINIMAP
		};

		virtual ~Tooltip() {}

		virtual void draw(Point<int16_t> cursorpos) const = 0;

		// Draw kept inside a screen of this size rather than the main one.
		//
		// A tooltip nudges itself back on-screen when it would overhang, and it
		// measures that against the main screen. On the lower panel - a
		// different size entirely - that clamp is against the wrong edges, so
		// the box runs off the bottom. Tooltips with no reason to care are
		// unaffected.
		virtual void draw_within(Point<int16_t> cursorpos, Point<int16_t>) const
		{
			draw(cursorpos);
		}
	};
}