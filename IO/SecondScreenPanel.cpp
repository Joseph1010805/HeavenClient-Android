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

#include "UI.h"

#include "UITypes/UIItemInventory.h"
#include "UITypes/UIWorldMap.h"

#include "../Constants.h"
#include "../Gameplay/Stage.h"

#include <cmath>
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
		// How far around an arrow a touch still counts. This was two thirds of
		// the way to the middle from each side, so taps meant for the map kept
		// turning pages instead.
		constexpr int16_t ARROW_REACH = 34;

		// The pages fill the panel, so there is no strip above them any more.
		constexpr int16_t CONTENT_TOP = 0;

		// How strongly a page's own backdrop shows through. It sits behind item
		// slots and icons, so it has to stay a background.
		constexpr float BACKDROP_FADE = 0.55f;
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

	UIElement* SecondScreenPanel::hosted(UIElement::Type type) const
	{
		for (auto& page : pages)
			if (page && page->get_type() == type)
				return page.get();

		return nullptr;
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

			// Its own tooltip still - the two maps must not fight over the
			// shared one - but drawn on the MAIN screen, which is bigger and
			// is not the thing being covered up. See draw_top_tooltip.
			map->set_panel_tooltip(&tooltip);
			slot = std::move(map);
			break;
		}
		case INVENTORY:
		{
			auto bag = std::make_unique<UIItemInventory>(Stage::get().get_player().get_inventory());
			bag->set_panel(panel_screen);
			slot = std::move(bag);
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

		// The pointer is here now, so it is not on the other screen.
		UI::get().set_cursor_visible(false);

		cursor_at = position;
		cursor_here = true;

		if (down)
		{
			touch_start = position;
			touching = true;

			pressed_arrow = arrow_at(position, screen);

			if (pressed_arrow != 0)
				return;
		}

		touch_now = position;

		if (up && pressed_arrow != 0)
		{
			touching = false;

			// Only if the finger lifted on the arrow it went down on.
			if (arrow_at(position, screen) == pressed_arrow)
				turn_to(current + pressed_arrow);

			pressed_arrow = 0;

			return;
		}

		if (!element || pressed_arrow != 0)
			return;

		// A finger is a cursor.
		//
		// The page is one of the game's own windows and already knows how to
		// behave under a pointer: moving over a place highlights it, clicking
		// travels. So the finger is passed on as a pointer and nothing here
		// interprets it - no counting taps, no deciding what a gesture meant.
		// Trying to do that is what stopped places highlighting, made a single
		// tap travel, and eventually crashed.
		//
		// A touch moves the pointer without pressing, so the first touch on a
		// place highlights it the way hovering does. The press is sent when the
		// finger lifts on somewhere already highlighted, which is the second
		// touch - so a place is read first and entered second.
		// Temporary, while the pointer is still going astray on the map.
		//
		// Every touch reports what it was, where the page thinks it is, how big
		// the page thinks it is, and what came back. When the pointer stops
		// lining up, the line logged at that moment says which of those changed
		// - rather than it having to be guessed at from a screenshot.
		{
			Point<int16_t> where = element ? element->get_position() : Point<int16_t>();
			Point<int16_t> size = element ? element->get_dimension() : Point<int16_t>();

			printf("[cursor] page=%d touch=%d,%d screen=%d,%d elem@%d,%d size=%dx%d %s\n",
				(int)current, position.x(), position.y(), screen.x(), screen.y(),
				where.x(), where.y(), size.x(), size.y(),
				down ? "DOWN" : (up ? "UP" : "move"));
		}

		// The touch is handed over in the PANEL's coordinates, not the window's.
		//
		// A window hit-tests by asking each of its buttons for bounds() at the
		// window's own position and checking the cursor against that, so it is
		// already accounting for where it sits - subtracting that here took it
		// off twice. The world map hid this because it fills the panel and so
		// sits at 0,0; the inventory is centred at 224,102 and was out by
		// exactly that much.
		Point<int16_t> at = position;

		// A clean slate before the page is asked, so what it sets is what gets
		// drawn - and so a second place can replace the first. MapTooltip
		// ignores a new name while its parent is unchanged, so without this the
		// first place hovered would be the only one it ever showed.
		//
		// Ours, not the shared one: the map on the top screen keeps whatever it
		// was showing.
		tooltip.reset();

		if (up)
		{
			touching = false;

			// One tap does it, everywhere except the world map.
			//
			// The map reads first and travels second on purpose: tapping a
			// place there takes you to another map, and doing that by accident
			// while trying to read a name is a nuisance. A button is not like
			// that - pressing EQUIP twice to equip once is just wrong.
			if (current != WORLDMAP)
			{
				element->send_cursor(true, at);

				cursor_state = element->send_cursor(false, at);
				highlighted = false;

				return;
			}

			bool same_place = highlighted && std::abs(highlight_at.x() - position.x()) < 24
				&& std::abs(highlight_at.y() - position.y()) < 24;

			if (same_place)
			{
				element->send_cursor(true, at);

				cursor_state = element->send_cursor(false, at);

				highlighted = false;
			}
			else
			{
				highlight_at = position;
				highlighted = true;

				// Still a hover, so the place under the finger stays lit and
				// the pointer keeps saying it can be clicked.
				cursor_state = element->send_cursor(false, at);
			}
		}
		else
		{
			// Moving, pressed or not, is a hover as far as the page is
			// concerned - which is what makes a region light up.
			cursor_state = element->send_cursor(false, at);
		}
	}

	void SecondScreenPanel::draw_top_tooltip() const
	{
		// Parked rather than following a cursor. The pointer that asked for
		// this is on the OTHER screen, so there is nowhere sensible on this one
		// for the box to hang off - a fixed corner is at least always in the
		// same place.
		tooltip.draw_within(Point<int16_t>(24, 60),
			Point<int16_t>(
				Constants::Constants::get().get_viewwidth(),
				Constants::Constants::get().get_viewheight()));
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
		// Half the artwork's size. At full size on a panel laid out this large
		// they were a third of the screen apart and covered the map.
		//
		// Drawn from the top-left corner, so half the drawn size comes off to
		// sit them on the middle of the side.
		Point<int16_t> full = arrow_left.get_dimensions();
		Point<int16_t> size = Point<int16_t>(full.x() / 2, full.y() / 2);

		int16_t mid = screen.y() / 2 - size.y() / 2;
		int16_t right = screen.x() - ARROW_INSET - size.x();

		Point<int16_t> at_left = Point<int16_t>(ARROW_INSET, mid);
		Point<int16_t> at_right = Point<int16_t>(right, mid);

		arrow_left.draw(DrawArgument(at_left, at_left, size, 1.0f, 1.0f, 1.0f, 0.0f));
		arrow_right.draw(DrawArgument(at_right, at_right, size, 1.0f, 1.0f, 1.0f, 0.0f));
	}

	const Texture* SecondScreenPanel::page_backdrop() const
	{
		// A page may bring its own backdrop instead of the panel's forest. The
		// inventory does: it is a bag, and it reads as one.
		if (current != INVENTORY)
			return nullptr;

		if (!backpack_tried)
		{
			backpack_tried = true;
			backpack = nl::nx::map001["Custom"]["InvBg"];
		}

		return backpack.is_valid() ? &backpack : nullptr;
	}

	void SecondScreenPanel::draw(Point<int16_t> screen) const
	{
		panel_screen = screen;

		// Behind the page, and dimmed - it is a background, and the slots and
		// item icons on top of it have to stay readable. BACKDROP_FADE is the
		// whole of that: turn it up to bring the bag forward.
		if (const Texture* backdrop = page_backdrop())
			backdrop->draw(DrawArgument(Point<int16_t>(0, 0), Point<int16_t>(0, 0),
				screen, 1.0f, 1.0f, BACKDROP_FADE, 0.0f));

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

		// Over everything else, because it is the thing being aimed with.
		//
		// There is one cursor between the two screens. If the main screen has
		// taken it back - it does that the moment it is touched - then it is
		// not here any more.
		// The hover box is NOT drawn here - see draw_top_tooltip. Reading it
		// meant looking down at the small screen while the thing it describes
		// is on the big one, and it covered a third of the map besides.
		if (cursor_here && !UI::get().is_cursor_visible())
			UI::get().draw_cursor_at(cursor_at, 1.0f, cursor_state);
	}
}
