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

#include "UITypes/UIMiniMap.h"
#include "UITypes/UIWorldMap.h"

#include "../Gameplay/Stage.h"

#include <cstdlib>
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
		// than a tap that wandered, or a drag meant for the page itself.
		constexpr int16_t SWIPE_THRESHOLD = 120;

		// The heading strip along the top, and the dots under it.
		constexpr int16_t HEADER_HEIGHT = 52;
		constexpr int16_t DOT = 10;
		constexpr int16_t DOT_SPACING = 26;
		constexpr int16_t DOT_Y = HEADER_HEIGHT + 10;
		constexpr int16_t CONTENT_TOP = DOT_Y + DOT + 12;
	}

	SecondScreenPanel::SecondScreenPanel()
		: current(WORLDMAP), touching(false), swiping(false), slide(0)
	{
		title = OutlinedText(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, Color::Name::TUNA);
		title.change_text(PAGE_NAMES[current]);
	}

	SecondScreenPanel::~SecondScreenPanel() {}

	SecondScreenPanel::Page SecondScreenPanel::page() const
	{
		return current;
	}

	UIElement* SecondScreenPanel::window() const
	{
		auto& slot = const_cast<std::unique_ptr<UIElement>&>(pages[current]);

		if (slot)
			return slot.get();

		// Built on first sight rather than up front, and not at all until a map
		// is loaded: most of these read the player, and at the login screen
		// there is no player to read.
		if (!Stage::get().is_active())
			return nullptr;

		switch (current)
		{
		case WORLDMAP:
			slot = std::make_unique<UIWorldMap>();
			break;
		case MINIMAP:
			slot = std::make_unique<UIMiniMap>(Stage::get().get_player().get_stats());
			break;
		default:
			// The remaining pages are not hosted here yet.
			break;
		}

		return slot.get();
	}

	Point<int16_t> SecondScreenPanel::window_position(Point<int16_t> screen) const
	{
		UIElement* element = window();

		if (!element)
			return Point<int16_t>(0, CONTENT_TOP);

		Point<int16_t> size = element->get_dimension();
		int16_t room = screen.y() - CONTENT_TOP;

		return Point<int16_t>(
			(screen.x() - size.x()) / 2,
			CONTENT_TOP + (room - size.y()) / 2);
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
		if (UIElement* element = window())
			element->update();

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

	void SecondScreenPanel::send_touch(Point<int16_t> position, Point<int16_t> screen, bool down, bool up)
	{
		UIElement* element = window();
		Point<int16_t> origin = window_position(screen);

		if (down)
		{
			touch_start = position;
			touch_now = position;
			touching = true;
			swiping = false;
			slide = 0;

			if (element)
				element->send_cursor(true, position - origin);

			return;
		}

		touch_now = position;

		if (touching)
		{
			int16_t travelled = touch_now.x() - touch_start.x();

			// Once it is a swipe it stays a swipe, so a finger dragged across
			// a map does not both scroll it and turn the page.
			if (!swiping && std::abs(travelled) >= SWIPE_THRESHOLD)
			{
				swiping = true;

				// Take the press back off the page it started on, or it is
				// left thinking a button is still held.
				if (element)
					element->send_cursor(false, touch_start - origin);
			}

			if (swiping)
				slide = travelled;
		}

		if (!swiping && element)
			element->send_cursor(!up, position - origin);

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

			swiping = false;
		}
	}

	void SecondScreenPanel::draw_chrome(Point<int16_t> screen) const
	{
		GraphicsGL::get().drawrectangle(
			0, 0, screen.x(), HEADER_HEIGHT, 0.0f, 0.0f, 0.0f, 0.5f);

		title.draw(Point<int16_t>(screen.x() / 2, 12));

		// One dot per page, the current one filled. It is the quickest way to
		// see both where you are and that there is more either side.
		int16_t total = DOT_SPACING * (NUM_PAGES - 1);
		int16_t left = (screen.x() - total) / 2;

		for (int16_t i = 0; i < NUM_PAGES; i++)
		{
			bool here = i == current;
			float shade = here ? 1.0f : 0.45f;

			GraphicsGL::get().drawrectangle(
				left + i * DOT_SPACING - DOT / 2, DOT_Y, DOT, DOT,
				shade, shade, shade, here ? 0.95f : 0.6f);
		}
	}

	void SecondScreenPanel::draw(Point<int16_t> screen) const
	{
		UIElement* element = window();

		if (element)
		{
			Point<int16_t> at = window_position(screen) + Point<int16_t>(slide, 0);

			element->set_position(at);
			element->draw(1.0f);
		}
		else
		{
			// A page with nothing behind it yet still shows its own space, so
			// swiping onto it reads as arriving somewhere rather than as the
			// panel having broken.
			GraphicsGL::get().drawrectangle(
				16 + slide, CONTENT_TOP, screen.x() - 32, screen.y() - CONTENT_TOP - 16,
				0.0f, 0.0f, 0.0f, 0.35f);
		}

		draw_chrome(screen);
	}
}
