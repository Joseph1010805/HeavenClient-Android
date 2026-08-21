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

#include <nlnx/nx.hpp>

namespace ms
{
	namespace
	{
		// The dots sit along the bottom. There is no heading: which page this
		// is, is obvious from what is on it.
		constexpr int16_t DOT = 7;
		constexpr int16_t DOT_SPACING = 18;
		constexpr int16_t DOT_BOTTOM = 10;

		// How far in the arrows sit, and how far around them a touch counts.
		constexpr int16_t ARROW_INSET = 4;
		constexpr int16_t ARROW_REACH = 60;

		// The pages fill the panel, so there is no strip above them any more.
		constexpr int16_t CONTENT_TOP = 0;
	}

	SecondScreenPanel::SecondScreenPanel()
		: current(WORLDMAP), touching(false), pressed_arrow(0)
	{
		// The same arrows the character select screen turns its pages with,
		// rather than a letter standing in for one.
		nl::node CharSelect = nl::nx::ui["Login.img"]["CharSelect"];

		arrow_left = CharSelect["pageL"]["normal"]["0"];
		arrow_right = CharSelect["pageR"]["normal"]["0"];

		loading = OutlinedText(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, Color::Name::TUNA);
		loading.change_text("Welcome to MapleStory DS");
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


	}

	int16_t SecondScreenPanel::arrow_at(Point<int16_t> position, Point<int16_t> screen) const
	{
		// A generous target either side - they are small marks, and a finger
		// is not.
		if (position.y() < screen.y() / 2 - ARROW_REACH || position.y() > screen.y() / 2 + ARROW_REACH)
			return 0;

		if (position.x() <= ARROW_REACH)
			return -1;

		if (position.x() >= screen.x() - ARROW_REACH)
			return 1;

		return 0;
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

			// An arrow turns the page. Everything else belongs to the page,
			// including a drag - scrolling a long map is what dragging is for
			// now that the arrows do the turning.
			pressed_arrow = arrow_at(position, screen);

			if (pressed_arrow == 0 && element)
				element->send_cursor(true, position - origin);

			return;
		}

		touch_now = position;

		if (up)
		{
			touching = false;

			if (pressed_arrow != 0)
			{
				// Only if the finger lifted on the arrow it went down on.
				if (arrow_at(position, screen) == pressed_arrow)
					turn_to(current + pressed_arrow);

				pressed_arrow = 0;

				return;
			}

			if (element)
				element->send_cursor(false, position - origin);

			return;
		}

		// A move is deliberately not passed on. A page here asks the main UI to
		// show its tooltips, and those then appear over the game on the other
		// screen - which is where that white list of NPC names was coming
		// from. There is no hovering on a touchscreen anyway.
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
		// An arrow is drawn from its top-left corner, so half its size comes
		// off to sit it on the middle of the side rather than below and right
		// of where it was asked for.
		Point<int16_t> size = arrow_left.get_dimensions();
		int16_t mid = screen.y() / 2 - size.y() / 2;

		arrow_left.draw(Point<int16_t>(ARROW_INSET, mid));
		arrow_right.draw(Point<int16_t>(screen.x() - ARROW_INSET - size.x(), mid));
	}

	void SecondScreenPanel::draw(Point<int16_t> screen) const
	{
		panel_screen = screen;

		UIElement* element = window();

		if (!Stage::get().is_active())
		{
			// Centred by measuring it: the centre alignment a Text can be given
			// needs a width to centre within, which this has not got.
			loading.draw(Point<int16_t>(
				(screen.x() - loading.width()) / 2,
				screen.y() / 2 - 8));

			return;
		}

		if (element)
		{
			Point<int16_t> at = window_position(screen);

			element->set_position(at);
			element->draw(1.0f);
		}
		else
		{
			// A page with nothing behind it yet still shows its own space, so
			// arriving there reads as arriving somewhere rather than as the
			// panel having broken.
			GraphicsGL::get().drawrectangle(
				16, CONTENT_TOP, screen.x() - 32, screen.y() - CONTENT_TOP - 16,
				0.0f, 0.0f, 0.0f, 0.35f);
		}

		draw_chrome(screen);
	}
}
