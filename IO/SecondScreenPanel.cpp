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
		// How far a finger has to travel before it counts as a swipe rather
		// than a tap that wandered, or a drag meant for the page itself.
		constexpr int16_t SWIPE_THRESHOLD = 120;

		// The dots sit along the bottom. There is no heading: which page this
		// is, is obvious from what is on it.
		constexpr int16_t DOT = 10;
		constexpr int16_t DOT_SPACING = 26;
		constexpr int16_t DOT_BOTTOM = 22;

		// The pages fill the panel, so there is no strip above them any more.
		constexpr int16_t CONTENT_TOP = 0;
	}

	SecondScreenPanel::SecondScreenPanel()
		: current(WORLDMAP), touching(false), swiping(false), slide(0)
	{
		arrow_left = OutlinedText(Text::Font::A15B, Text::Alignment::LEFT, Color::Name::YELLOW, Color::Name::TUNA);
		arrow_left.change_text("<");

		arrow_right = OutlinedText(Text::Font::A15B, Text::Alignment::RIGHT, Color::Name::YELLOW, Color::Name::TUNA);
		arrow_right.change_text(">");
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
		{
			auto map = std::make_unique<UIWorldMap>();
			map->set_panel(panel_screen);
			slot = std::move(map);
			break;
		}
		case MINIMAP:
		{
			auto map = std::make_unique<UIMiniMap>(Stage::get().get_player().get_stats());
			map->set_panel(panel_screen);
			slot = std::move(map);
			break;
		}
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

		// A page that already fills the panel wants the corner, not centring -
		// centring something the size of the screen only moves it off it.
		if (size.x() >= screen.x() && size.y() >= screen.y())
			return Point<int16_t>(0, 0);

		int16_t room = screen.y() - CONTENT_TOP;

		return Point<int16_t>(
			(screen.x() - size.x()) / 2,
			CONTENT_TOP + (room - size.y()) / 2);
	}

	void SecondScreenPanel::turn_to(int16_t next)
	{
		// The deck wraps, so the last page is one swipe from the first either
		// way round rather than seven.
		if (next < 0)
			next = NUM_PAGES - 1;
		else if (next >= NUM_PAGES)
			next = 0;

		current = static_cast<Page>(next);
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
		panel_screen = screen;

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
		// One dot per page, the current one filled, along the bottom.
		int16_t total = DOT_SPACING * (NUM_PAGES - 1);
		int16_t left = (screen.x() - total) / 2;
		int16_t y = screen.y() - DOT_BOTTOM;

		for (int16_t i = 0; i < NUM_PAGES; i++)
		{
			bool here = i == current;
			float shade = here ? 1.0f : 0.45f;

			GraphicsGL::get().drawrectangle(
				left + i * DOT_SPACING - DOT / 2, y, DOT, DOT,
				shade, shade, shade, here ? 0.95f : 0.6f);
		}

		// A mark at each side saying there is more that way. Small, yellow and
		// out of the way - the page itself is what matters.
		arrow_left.draw(Point<int16_t>(14, screen.y() / 2));
		arrow_right.draw(Point<int16_t>(screen.x() - 14, screen.y() / 2));
	}

	void SecondScreenPanel::draw(Point<int16_t> screen) const
	{
		panel_screen = screen;

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
