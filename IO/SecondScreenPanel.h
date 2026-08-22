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

#include "Components/MapTooltip.h"

#include "../Graphics/SpecialText.h"
#include "../Graphics/Animation.h"
#include "../Graphics/Texture.h"
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
			INVENTORY,
			EQUIPMENT,
			ABILITY,
			SKILLS,
			QUESTS,
			HOTKEYS,
			CHAT,
			NUM_PAGES
		};

		SecondScreenPanel();
		~SecondScreenPanel();

		void draw(Point<int16_t> screen) const;

		// Draw the hovered thing's information on the MAIN screen, in that
		// screen's coordinates. Called from the main pass, not this one.
		void draw_top_tooltip() const;
		void update();

		// Play the level-up flourish over whatever page is showing.
		void play_levelup();

		// A touch in panel pixels.
		void send_touch(Point<int16_t> position, Point<int16_t> screen, bool down, bool up);

		Page page() const;

		// The hosted window of this type, if one has been built. Null
		// otherwise - this never builds one, it only reports.
		UIElement* hosted(UIElement::Type type) const;

	private:
		void turn_to(int16_t next);

		// The heading and the row of dots, so it is always clear which page
		// this is and how many there are.
		void draw_chrome(Point<int16_t> screen) const;

		// Which arrow a point is on: -1 back, 1 forward, 0 neither.
		int16_t arrow_at(Point<int16_t> position, Point<int16_t> screen) const;

		// Everything the panel points at.
		//
		// The client has ONE cursor and ONE map tooltip, and while a map is
		// open on each screen at once both copies write to them and each wipes
		// the other's - which is the pointer losing track of what it is over.
		// So the panel keeps its own of each and never touches the shared ones
		// except to say which screen the pointer is on.
		mutable MapTooltip tooltip;
		Cursor::State cursor_state = Cursor::State::IDLE;

		// The page's window, built the first time that page is shown. They are
		// the game's own windows, but owned here rather than by UI - a window
		// in UI's list is drawn over the game, which is the thing this panel
		// exists to avoid.
		UIElement* window() const;

		// A backdrop belonging to this page rather than to the panel, or null
		// when the page is happy with the panel's own.
		const Texture* page_backdrop() const;

		// Where that window sits: centred in the room below the heading.
		Point<int16_t> window_position(Point<int16_t> screen) const;

		Page current;

		std::unique_ptr<UIElement> pages[NUM_PAGES];

		// Where a drag started and whether one is in progress, which is all a
		// swipe is until the finger lifts.
		Point<int16_t> touch_start;
		Point<int16_t> touch_now;
		bool touching;

		// Which arrow is being held, -1 for back and 1 for forward, 0 for
		// none. The arrows turn pages now; a drag belongs entirely to the page
		// under it, which is what scrolling a long map needs.
		int16_t pressed_arrow;

		// Where a place was last highlighted, so lifting a finger on the same
		// place is the click and the one before it was only the hover.
		Point<int16_t> highlight_at;
		bool highlighted = false;

		// Where the pointer is on this panel, and whether it has been put here
		// at all yet. The pages are the game's own windows and expect a cursor
		// to aim with - picking a dot out of a crowded map needs to show which
		// one is about to be picked.
		Point<int16_t> cursor_at;
		bool cursor_here = false;

		// How big the panel is, remembered so a page can be told at the moment
		// it is built rather than only when it is drawn. Mutable because
		// drawing is where the size arrives and drawing is const.
		mutable Point<int16_t> panel_screen;

		// The marks either side saying there is more that way - the character
		// select screen's own page arrows.
		Texture arrow_left;
		Texture arrow_right;

		// A page's own backdrop, loaded the first time that page is shown.
		mutable Texture backdrops[NUM_PAGES];

		// The level-up flourish, and whether it is running. Loaded the first
		// time somebody levels rather than at startup - it is 64 frames, and
		// most sessions never see it.
		mutable Animation levelup;
		mutable bool levelup_tried = false;
		bool levelup_playing = false;

		// Shown while there is no map loaded and so no page to show.
		OutlinedText loading;
	};
}
