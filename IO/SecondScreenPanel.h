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

#include "UIElement.h"

#include "../Graphics/SpecialText.h"
#include "../Template/Point.h"

#include <memory>

namespace ms
{
	// What the lower panel shows.
	//
	// Not a menu of buttons but a deck of pages, swiped between the way a
	// handheld's lower screen usually works. The game opens on the world map;
	// everything else is a swipe away and stays where it was left.
	class SecondScreenPanel
	{
	public:
		enum Page
		{
			WORLDMAP,
			MINIMAP,
			INVENTORY,
			EQUIPMENT,
			STATS,
			QUESTS,
			HOTKEYS,
			CHAT,
			NUM_PAGES
		};

		SecondScreenPanel();
		~SecondScreenPanel();

		void draw(Point<int16_t> screen) const;
		void update();

		// A touch in panel pixels.
		void send_touch(Point<int16_t> position, Point<int16_t> screen, bool down, bool up);

		Page page() const;

	private:
		void turn_to(int16_t next);

		// The heading and the row of dots, so it is always clear which page
		// this is and how many there are.
		void draw_chrome(Point<int16_t> screen) const;

		// The page's window, built the first time that page is shown. They are
		// the game's own windows, but owned here rather than by UI - a window
		// in UI's list is drawn over the game, which is the thing this panel
		// exists to avoid.
		UIElement* window() const;

		// Where that window sits: centred in the room below the heading.
		Point<int16_t> window_position(Point<int16_t> screen) const;

		Page current;

		std::unique_ptr<UIElement> pages[NUM_PAGES];

		// Where a drag started and whether one is in progress, which is all a
		// swipe is until the finger lifts.
		Point<int16_t> touch_start;
		Point<int16_t> touch_now;
		bool touching;

		// Whether this drag has already been decided to be a swipe. Once it
		// has, the page under it stops seeing the movement, so dragging across
		// a map cannot both scroll it and turn the page.
		bool swiping;

		// How far the page is slid across while a swipe is under way, so the
		// gesture is visible rather than the page simply changing on release.
		int16_t slide;

		OutlinedText title;
	};
}
