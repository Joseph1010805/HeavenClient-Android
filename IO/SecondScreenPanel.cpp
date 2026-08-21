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
#include "SecondScreenPanel.h"

#include "SecondScreen.h"

#include "../Graphics/GraphicsGL.h"

namespace ms
{
	namespace
	{
		const char* PAGE_NAMES[SecondScreenPanel::NUM_PAGES] = {
			"World Map",
			"Mini Map",
			"Inventory",
			"Equipment",
			"Stats",
			"Quests",
			"Hotkeys",
			"Chat"
		};

		// How far a finger has to travel before it counts as a swipe rather
		// than a tap that wandered.
		constexpr int16_t SWIPE_THRESHOLD = 70;

		// The heading strip along the top, and the dots under it.
		constexpr int16_t HEADER_HEIGHT = 34;
		constexpr int16_t DOT = 6;
		constexpr int16_t DOT_SPACING = 16;
		constexpr int16_t DOT_Y = HEADER_HEIGHT + 8;
	}

	SecondScreenPanel::SecondScreenPanel()
		: current(WORLDMAP), touching(false), slide(0)
	{
		title = OutlinedText(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, Color::Name::TUNA);
		title.change_text(PAGE_NAMES[current]);
	}

	SecondScreenPanel::Page SecondScreenPanel::page() const
	{
		return current;
	}

	void SecondScreenPanel::turn_to(int16_t next)
	{
		// The deck does not wrap. Running off either end should feel like the
		// end, not like being thrown back to the other side.
		if (next < 0)
			next = 0;
		else if (next >= NUM_PAGES)
			next = NUM_PAGES - 1;

		current = static_cast<Page>(next);
		title.change_text(PAGE_NAMES[current]);
	}

	void SecondScreenPanel::update()
	{
		// Ease the slide back to nothing once the finger is gone, so a swipe
		// that did not travel far enough springs back instead of sticking.
		if (!touching && slide != 0)
		{
			int16_t step = slide / 4;

			if (step == 0)
				step = slide > 0 ? 1 : -1;

			slide -= step;
		}
	}

	void SecondScreenPanel::send_touch(Point<int16_t> position, bool down, bool up)
	{
		if (down)
		{
			touch_start = position;
			touch_now = position;
			touching = true;
			slide = 0;

			return;
		}

		touch_now = position;

		if (touching)
			slide = touch_now.x() - touch_start.x();

		if (up)
		{
			touching = false;

			int16_t travelled = touch_now.x() - touch_start.x();

			// Dragging left brings the next page in from the right, which is
			// the way a stack of cards moves under a finger.
			if (travelled <= -SWIPE_THRESHOLD)
				turn_to(current + 1);
			else if (travelled >= SWIPE_THRESHOLD)
				turn_to(current - 1);
		}
	}

	void SecondScreenPanel::draw_chrome() const
	{
		GraphicsGL::get().drawrectangle(
			0, 0, SecondScreen::WIDTH, HEADER_HEIGHT, 0.0f, 0.0f, 0.0f, 0.5f);

		title.draw(Point<int16_t>(SecondScreen::WIDTH / 2, 7));

		// One dot per page, the current one filled. It is the quickest way to
		// see both where you are and that there is more either side.
		int16_t total = DOT_SPACING * (NUM_PAGES - 1);
		int16_t left = (SecondScreen::WIDTH - total) / 2;

		for (int16_t i = 0; i < NUM_PAGES; i++)
		{
			bool here = i == current;
			float shade = here ? 1.0f : 0.45f;

			GraphicsGL::get().drawrectangle(
				left + i * DOT_SPACING - DOT / 2, DOT_Y, DOT, DOT,
				shade, shade, shade, here ? 0.95f : 0.6f);
		}
	}

	void SecondScreenPanel::draw() const
	{
		// The page's own area, below the heading. Pages are given this space
		// and nothing outside it.
		constexpr int16_t top = DOT_Y + DOT + 8;

		GraphicsGL::get().drawrectangle(
			8 + slide, top, SecondScreen::WIDTH - 16, SecondScreen::HEIGHT - top - 8,
			0.0f, 0.0f, 0.0f, 0.35f);

		draw_chrome();
	}
}
