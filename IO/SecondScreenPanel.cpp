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

#include "UITypes/UIHotkeys.h"
#include "UITypes/UICharacterPage.h"

#include "UI.h"

#include "UITypes/UIEquipInventory.h"
#include "UITypes/UIItemInventory.h"
#include "UITypes/UIQuestLog.h"
#include "UITypes/UISkillbook.h"
#include "UITypes/UIStatsinfo.h"
#include "UITypes/UIWorldMap.h"

#include "../Configuration.h"
#include "../Voice.h"
#include "../Constants.h"
#include "../Gameplay/Stage.h"
#include "../Character/ExpTable.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
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

		// Where the page's name sits. Between the two arrows, which are inset
		// at the same height on either side, so a centred title clears both.
		// Under the experience bar, which now runs along the very top edge.
		constexpr int16_t TITLE_TOP = 17;

	// THE CHROME ROW: the address bar at one end, the clock at the other.
	//
	// One top and one size for both, so they cannot drift out of line with
	// each other. A size up from 18 - these are read at arm's length from a
	// screen nineteen inches away, not clicked with a mouse.
	constexpr int16_t CRUMB = 22;
	constexpr int16_t CHROME_TOP = 3;

	// How much of the gauge artwork is rim rather than trough, so the fill
	// can be drawn inside it instead of over it.
	constexpr int16_t RIM = 2;

		// How far in the arrows sit, and how far around them a touch counts.
		constexpr int16_t ARROW_INSET = 4;
		// How far around an arrow a touch still counts. This was two thirds of
		// the way to the middle from each side, so taps meant for the map kept
		// turning pages instead.
		constexpr int16_t ARROW_REACH = 34;

		// The pages fill the panel, so there is no strip above them any more.
		constexpr int16_t CONTENT_TOP = 0;

		// HOW FAR THE MAP'S BACKGROUND IS KNOCKED BACK.
		//
		// It is scenery, not a picture being looked at: item icons, stat
		// numbers and a quest list all have to stay readable on top of it, and
		// the maps run from noon skies to caves. One number, here, so it can
		// be tuned in one place.
		constexpr float BACKDROP_DIM = 0.55f;

		// How strongly a page's own backdrop shows through. It sits behind item
		// slots and icons, so it has to stay a background.
		constexpr float BACKDROP_FADE = 0.55f;
	}

	SecondScreenPanel::SecondScreenPanel()
		: current(HOME), touching(false), pressed_arrow(0)
	{
		// The same arrows the character select screen turns its pages with,
		// rather than a letter standing in for one.
		nl::node CharSelect = nl::nx::ui["Login.img"]["CharSelect"];

		home_icon = nl::nx::map["MapHelper.img"]["mark"]["Henesys"];
		tile_frame = nl::nx::ui["Login.img"]["Notice"]["backgrnd"]["0"];

		hp_text = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::WHITE);
		mp_text = Text(Text::Font::A11B, Text::Alignment::RIGHT, Color::Name::WHITE);
		exp_text = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE);

		arrow_left = CharSelect["pageL"]["normal"]["0"];
		arrow_right = CharSelect["pageR"]["normal"]["0"];


	}

	SecondScreenPanel::~SecondScreenPanel() {}

	SecondScreenPanel::Page SecondScreenPanel::page() const
	{
		return current;
	}

	bool SecondScreenPanel::is_menu(Page page)
	{
		return page == HOME || page == CHARACTER || page == ADVENTURE
			|| page == CHAT;
	}

	const SecondScreenPanel::Page* SecondScreenPanel::menu_items(Page page, size_t& count)
	{
		// SIX, in three rows of two.
		static const Page home[] = {
			CHARACTER, ADVENTURE, HOTKEYS, CHAT, SETTINGS, MINIGAME
		};

		// WHO you are: the three pages about the person.
		//
		// Quests left here for ADVENTURE. A quest is not a fact about your
		// character, it is somewhere you are going - and it sat next to your
		// stats only because there were four tabs on the top screen.
		static const Page character[] = { INVENTORY, EQUIPMENT, ABILITY };

		// WHERE you are going: the map and the quest log answer the same
		// question from two directions.
		static const Page adventure[] = { WORLDMAP, QUESTS };

		// TALKING: out loud, and in writing.
		static const Page social[] = { SHOUT, ROOMCHAT };

		switch (page)
		{
		case HOME:
			count = sizeof(home) / sizeof(home[0]);
			return home;
		case CHARACTER:
			count = sizeof(character) / sizeof(character[0]);
			return character;
		case ADVENTURE:
			count = sizeof(adventure) / sizeof(adventure[0]);
			return adventure;
		case CHAT:
			count = sizeof(social) / sizeof(social[0]);
			return social;
		default:
			count = 0;
			return nullptr;
		}
	}

	void SecondScreenPanel::go_to(Page which)
	{
		if (which < 0 || which >= NUM_PAGES || which == current)
			return;

		trail.push_back(current);
		current = which;

		leave_page();
		carried = Keyboard::Mapping();
	}

	void SecondScreenPanel::go_back()
	{
		if (trail.empty())
			return;

		current = trail.back();
		trail.pop_back();

		leave_page();
	}

	void SecondScreenPanel::go_home()
	{
		trail.clear();
		current = HOME;

		leave_page();
	}

	void SecondScreenPanel::show_page(Page which)
	{
		go_to(which);
	}

	UIElement* SecondScreenPanel::hosted(UIElement::Type type) const
	{
		for (auto& page : pages)
			if (page && page->get_type() == type)
				return page.get();

		return nullptr;
	}

	Keyboard::Mapping SecondScreenPanel::selected_mapping() const
	{
		// Only a page that has been built can have a selection, so this must
		// not go through window() - that builds the page on first sight, and
		// merely asking what is selected should not bring one into existence.
		auto& slot = pages[current];

		return slot ? slot->selected_mapping() : Keyboard::Mapping();
	}

	void SecondScreenPanel::send_scroll(double yoffset)
	{
		if (UIElement* element = window())
			element->send_scroll(yoffset);
	}

	void SecondScreenPanel::show_guest(UIElement* element, const char* backdrop)
	{
		guest = element;
		guest_backdrop_name = backdrop;
		guest_backdrop_tex = Texture();
	}

	void SecondScreenPanel::clear_guest()
	{
		guest = nullptr;
		guest_backdrop_name = nullptr;
		guest_backdrop_tex = Texture();
	}

	UIElement* SecondScreenPanel::window() const
	{
		// A guest outranks the page under it.
		if (guest)
			return guest;

		// A menu is buttons and nothing else.
		//
		// SETTINGS and MINIGAME have no window behind them yet and show as
		// empty pages. Settings deliberately does NOT host UIOptionMenu: that
		// window has no set_panel, so it lays itself out for the 800x600 top
		// screen and would arrive here two and a half times too big. A blank
		// page is honest; a window spilling off three edges is not.
		if (is_menu(current) || current == MINIGAME || current == SETTINGS
			|| current == SHOUT)
			return nullptr;

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
		case EQUIPMENT:
		{
			auto worn = std::make_unique<UIEquipInventory>(Stage::get().get_player().get_inventory());
			worn->set_panel(panel_screen);
			slot = std::move(worn);
			break;
		}
		case ABILITY:
		{
			// Stats AND skills, one scrollable column.
			auto page = std::make_unique<UICharacterPage>(
				Stage::get().get_player().get_stats(),
				Stage::get().get_player().get_skills());

			page->set_panel(panel_screen);
			slot = std::move(page);
			break;
		}
		case QUESTS:
		{
			auto log = std::make_unique<UIQuestLog>(
				Stage::get().get_player().get_quests());

			log->set_panel(panel_screen);
			slot = std::move(log);
			break;
		}
		case HOTKEYS:
		{
			// No set_panel: the others are the game's own windows being
			// borrowed and need telling they are down here. This one has no
			// life on the main screen, so it is already in panel coordinates.
			slot = std::make_unique<UIHotkeys>();
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

	void SecondScreenPanel::play_levelup()
	{
		if (!levelup_tried)
		{
			levelup_tried = true;
			levelup = nl::nx::map001["Custom"]["LevelUp"];
		}

		levelup.reset();
		levelup_playing = true;
	}

	void SecondScreenPanel::leave_page()
	{
		// Nothing a page put on screen outlives the page. A place name from the
		// map, an item's details, whatever was picked up - all of it describes
		// something that is no longer being shown, and the map's box in
		// particular sits on the OTHER screen where there is nothing to say it
		// is stale.
		tooltip.reset();

		UI::get().clear_tooltip(Tooltip::Parent::WORLDMAP);
		UI::get().clear_tooltip(Tooltip::Parent::MINIMAP);
		UI::get().clear_tooltip(Tooltip::Parent::ITEMINVENTORY);
		UI::get().clear_tooltip(Tooltip::Parent::EQUIPINVENTORY);
		UI::get().clear_tooltip(Tooltip::Parent::SKILLBOOK);
	}

	void SecondScreenPanel::turn_to(int16_t next)
	{
		// Kept for anything still asking to be taken straight somewhere.
		if (next >= 0 && next < NUM_PAGES)
			go_to(static_cast<Page>(next));
	}

	void SecondScreenPanel::update()
	{
		if (levelup_playing && levelup.update())
			levelup_playing = false;

		// Nothing announces a shop closing, so notice it here.
		if (guest && !guest->is_active())
			clear_guest();

		if (UIElement* element = window())
			element->update();

		// Remember the last thing any page picked out. turn_to() clears a
		// page's own selection, so without this the potion you chose on the
		// item page is forgotten by the time you reach the hotkey page - and
		// carrying it there is the entire point.
		if (current != HOTKEYS)
		{
			Keyboard::Mapping now = selected_mapping();

			if (now.type != KeyType::Id::NONE)
				carried = now;
		}


	}

	int16_t SecondScreenPanel::arrow_at(Point<int16_t> position, Point<int16_t> screen) const
	{
		// A generous target either side - they are small marks, and a finger
		// is not.
		// Along the TOP now, not the sides - a page fills the panel and its own
		// content wants the middle of the edges.
		if (position.y() > ARROW_REACH)
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

			// THE ADDRESS BAR IS THE WAY BACK.
			//
			// Pressing any icon in the trail returns to that folder, so the
			// row that says where you are is also how you leave. Tested
			// before the page, or the window underneath swallows it.
			for (size_t i = 0; i < crumb_count(); i++)
			{
				if (!crumb_box(i).contains(position))
					continue;

				// The last crumb is where we already are.
				if (i < trail.size())
				{
					current = trail[i];
					trail.resize(i);

					leave_page();
					carried = Keyboard::Mapping();
				}

				touching = false;

				return;
			}

			// PUSH TO TALK.
			//
			// The first press switches the socket on; after that the button is
			// held. Both live here rather than on a settings toggle because
			// the page has one control on it and it should do the obvious
			// thing the first time it is pressed.
			if (current == SHOUT && talk_box(screen).contains(position))
			{
				Voice& voice = Voice::get();

				if (!voice.is_open())
					voice.start();
				else
					voice.set_talking(true);

				touching = false;

				return;
			}

			// Tested before the page gets the touch, or the window underneath
			// swallows it.
			if (hotkey_jump_visible() && hotkey_jump_box(screen).contains(position))
			{
				go_to(HOTKEYS);
				touching = false;

				return;
			}
		}

		// LET GO ANYWHERE, not just on the button.
		//
		// A thumb slides while it presses. Ending transmission only on a
		// release inside the box means a finger that drifted off the edge
		// leaves the microphone live, which is the one bug this page must
		// not have.
		if (up)
			Voice::get().set_talking(false);

		touch_now = position;

		// A VERTICAL DRAG SCROLLS.
		//
		// The stats page and the skill list are taller than the panel and
		// could only be moved with a thumbstick, which is not where a hand
		// is when it is already on the glass. Read as a delta between calls
		// and handed to the page as the wheel signal it already understands.
		//
		// Only once the finger has committed to going up or down: a sideways
		// swipe must stay a page gesture, not a scroll with a nudge in it.
		if (down)
		{
			drag_y = position.y();
			dragging = false;
		}
		else if (!up && touching)
		{
			int16_t dx0 = static_cast<int16_t>(position.x() - touch_start.x());
			int16_t dy0 = static_cast<int16_t>(position.y() - touch_start.y());

			int16_t adx0 = dx0 < 0 ? -dx0 : dx0;
			int16_t ady0 = dy0 < 0 ? -dy0 : dy0;

			if (!dragging && ady0 > 8 && ady0 > adx0)
				dragging = true;

			if (dragging)
			{
				int16_t step = static_cast<int16_t>(position.y() - drag_y);

				if (step != 0)
				{
					if (UIElement* page = window())
						page->send_scroll(step / 24.0);

					drag_y = position.y();
				}
			}
		}

		// THE GESTURES.
		//
		// Read on the way UP, and only when the finger travelled further
		// sideways than down - so a swipe cannot be mistaken for a scroll, and
		// a tap that wanders a few pixels is still a tap.
		//
		//   left  -> right   BACK one level, as on a phone
		//   right -> left    HOME, all the way to the root
		if (up && touching)
		{
			int16_t dx = static_cast<int16_t>(position.x() - touch_start.x());
			int16_t dy = static_cast<int16_t>(position.y() - touch_start.y());

			int16_t adx = dx < 0 ? -dx : dx;
			int16_t ady = dy < 0 ? -dy : dy;

			if (adx >= SWIPE_MIN && adx > ady)
			{
				touching = false;

				// A guest is a window in FRONT of the tree, not a place in it.
				// Either swipe shuts it - there is nowhere else for a swipe to
				// go while a shop is open, and both directions meaning "close"
				// is easier than remembering which.
				if (guest)
				{
					guest->deactivate();
					clear_guest();
				}
				else if (dx > 0)
				{
					// ON THE MAP, BACK MEANS OUT ONE REGION.
					//
					// Victoria Island opens onto Henesys; swiping back there
					// used to leave the map entirely, which threw away the
					// step you had just taken to get in. It leaves the map
					// only once there is nowhere further out to go - so the
					// gesture means the same thing at every depth, and the
					// address tree is what it lands on at the top.
					bool climbed = false;

					if (current == WORLDMAP)
						if (auto* map = dynamic_cast<UIWorldMap*>(window()))
							climbed = map->step_out();

					if (!climbed)
						go_back();
				}
				else
				{
					go_home();
				}

				return;
			}
		}

		// A menu is buttons and nothing else, so it never reaches a window.
		if (is_menu(current))
		{
			if (up && touching)
			{
				int16_t hit = menu_at(position, screen);

				size_t count = 0;
				const Page* items = menu_items(current, count);

				if (hit >= 0 && items)
					go_to(items[hit]);
			}

			if (up)
				touching = false;

			return;
		}

		if (!element)
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

			// A finger that travelled was dragging, not pointing.
			//
			// Releasing at the end of a drag must never press whatever happens
			// to be under it: panning the map, or running a finger across the
			// inventory, would otherwise act on wherever it came to rest. A
			// click has to be deliberate, which means going down and coming up
			// in the same place.
			int16_t moved_x = std::abs(position.x() - touch_start.x());
			int16_t moved_y = std::abs(position.y() - touch_start.y());

			if (moved_x > DRAG_SLOP || moved_y > DRAG_SLOP)
			{
				highlighted = false;

				// Still a hover, so whatever it ended over stays lit.
				cursor_state = element->send_cursor(false, at);

				return;
			}

			// One tap does it, everywhere except the world map.
			//
			// The map reads first and travels second on purpose: tapping a
			// place there takes you to another map, and doing that by accident
			// while trying to read a name is a nuisance. A button is not like
			// that - pressing EQUIP twice to equip once is just wrong.
			// The two-step is for places on the map, not for the controls
			// around it. Back, the page arrows and the navigation buttons all
			// act on the first tap like every other button in the game.
			if (current != WORLDMAP || element->button_at(at))
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
		// Only while the map is the page being shown. The box describes a place
		// on that map, so once the panel has moved on it is describing nothing.
		if (current != WORLDMAP)
			return;

		// Parked rather than following a cursor. The pointer that asked for
		// this is on the OTHER screen, so there is nowhere sensible on this one
		// for the box to hang off - a fixed corner is at least always in the
		// same place.
		tooltip.draw_within(Point<int16_t>(6, 22),
			Point<int16_t>(
				Constants::Constants::get().get_viewwidth(),
				Constants::Constants::get().get_viewheight()));
	}

	Rectangle<int16_t> SecondScreenPanel::menu_box(size_t index, size_t count,
		Point<int16_t> screen) const
	{
		// Two across, as many rows as it takes. Big enough that the label can
		// be read at arm's length and hit without looking.
		constexpr int16_t COLS = 2;
		constexpr int16_t MARGIN = 18;
		constexpr int16_t GAP = 16;
		constexpr int16_t TOP = 46;

		int16_t rows = static_cast<int16_t>((count + COLS - 1) / COLS);
		int16_t w = (screen.x() - MARGIN * 2 - GAP * (COLS - 1)) / COLS;
		int16_t h = (screen.y() - TOP - MARGIN - GAP * (rows - 1)) / rows;

		int16_t col = static_cast<int16_t>(index % COLS);
		int16_t row = static_cast<int16_t>(index / COLS);

		Point<int16_t> at(MARGIN + col * (w + GAP), TOP + row * (h + GAP));

		return Rectangle<int16_t>(at, at + Point<int16_t>(w, h));
	}

	int16_t SecondScreenPanel::menu_at(Point<int16_t> position, Point<int16_t> screen) const
	{
		size_t count = 0;
		const Page* items = menu_items(current, count);

		for (size_t i = 0; i < count; i++)
			if (menu_box(i, count, screen).contains(position))
				return static_cast<int16_t>(i);

		return -1;
	}

	void SecondScreenPanel::draw_vitals(Point<int16_t> screen) const
	{
		if (!Stage::get().is_active())
			return;

		const CharStats& stats = Stage::get().get_player().get_stats();

		int32_t hp = stats.get_stat(Maplestat::Id::HP);
		int32_t maxhp = stats.get_total(Equipstat::Id::HP);
		int32_t mp = stats.get_stat(Maplestat::Id::MP);
		int32_t maxmp = stats.get_total(Equipstat::Id::MP);

		float hp_ratio = maxhp > 0 ? static_cast<float>(hp) / maxhp : 0.0f;
		float mp_ratio = maxmp > 0 ? static_cast<float>(mp) / maxmp : 0.0f;

		pulse++;

		// How hard a bar breathes. Full is steady; the emptier it gets the
		// faster and deeper it pulses, so running low is something you notice
		// out of the corner of your eye rather than something you have to read.
		auto breathe = [this](float ratio) -> float
		{
			if (ratio >= 0.5f)
				return 1.0f;

			float urgency = (0.5f - ratio) * 2.0f;          // 0 at half, 1 at empty
			float speed = 0.04f + urgency * 0.16f;
			float wave = std::sin(pulse * speed) * 0.5f + 0.5f;

			return 1.0f - urgency * 0.55f * wave;
		};

		int16_t top = 0;
		int16_t height = screen.y();

		// THE FOUR MARGINS.
		//
		// HP up the left, MP up the right, an empty strip along the top that
		// carries the clock, and the EXP bar along the bottom. All VITAL_W
		// thick, so the page sits in an even frame.
		//
		// Four-sided HP/MP bars were tried on 31 Aug and reverted: a gauge
		// that turns a corner cannot be read at a glance, and at full it is a
		// slab. Each gauge owns ONE side now.
		int16_t W = screen.x();
		int16_t Hh = screen.y();

		// From the very top down to the EXP bar. There is no top strip any
		// more - see below - so a side gauge owns its whole edge.
		int16_t inner_top = 0;
		int16_t inner_h = static_cast<int16_t>(Hh - VITAL_W);

		int16_t hp_h = static_cast<int16_t>(inner_h * hp_ratio);
		int16_t mp_h = static_cast<int16_t>(inner_h * mp_ratio);

		// MUTED AT REST, VIVID WHEN LOW.
		//
		// The old bars did the opposite - they DARKENED as they emptied, so
		// the moment you most needed to see one was the moment it faded into
		// the frame. Now full is a dusty, low-saturation colour that sits
		// quietly behind the page, and emptying mixes it toward the vivid
		// version in time with the pulse. Vibrancy IS the warning.
		//
		// Red and blue mix at DIFFERENT rates on purpose: matched, the two
		// would beat together and neither would say which of them is low.
		auto mix = [this](float ratio, float speed,
			float mr, float mg, float mb, float vr, float vg, float vb,
			float& r, float& g, float& b)
		{
			float heat = 0.0f;

			if (ratio < 0.5f)
			{
				float urgency = (0.5f - ratio) * 2.0f;   // 0 at half, 1 at empty
				float wave = std::sin(pulse * speed) * 0.5f + 0.5f;

				heat = urgency * wave;
			}

			r = mr + (vr - mr) * heat;
			g = mg + (vg - mg) * heat;
			b = mb + (vb - mb) * heat;
		};

		float hr, hg, hb, mr2, mg2, mb2;

		mix(hp_ratio, 0.085f, 0.55f, 0.30f, 0.29f, 1.00f, 0.22f, 0.20f, hr, hg, hb);
		mix(mp_ratio, 0.052f, 0.32f, 0.40f, 0.55f, 0.25f, 0.60f, 1.00f, mr2, mg2, mb2);

		// THE CHANNEL IS ARTWORK, not a drawn rectangle.
		//
		// A rounded silver rim round a black trough, stretched to whatever
		// length the edge needs. The vertical copy is a separate bitmap
		// rotated at build time - see BAR_IMAGE in make_assets.py.
		if (!bar_v.is_valid())
			bar_v = nl::nx::map001["Custom"]["BarV"];

		if (!bar_h.is_valid())
			bar_h = nl::nx::map001["Custom"]["BarH"];

		channel(bar_v, 0, inner_top, VITAL_W, inner_h);
		channel(bar_v, W - VITAL_W, inner_top, VITAL_W, inner_h);

		// Filled from the bottom, the way a vial empties - and INSIDE the
		// rim, so the frame stays a frame.
		bar(RIM, static_cast<int16_t>(inner_top + inner_h - hp_h + RIM),
			VITAL_W - RIM * 2, static_cast<int16_t>(hp_h - RIM * 2),
			hr, hg, hb, 0.90f);
		bar(W - VITAL_W + RIM, static_cast<int16_t>(inner_top + inner_h - mp_h + RIM),
			VITAL_W - RIM * 2, static_cast<int16_t>(mp_h - RIM * 2),
			mr2, mg2, mb2, 0.90f);

		// NO TOP STRIP.
		//
		// It was a black band across the whole width holding nothing but the
		// time, and it cut the panel's own frame off at the top - so the page
		// began below a bar that was not a gauge and did not need to be there.
		// The clock and the address bar sit straight on the frame now.
		if (!clock_icon.is_valid())
			clock_icon = nl::nx::map001["Custom"]["IconTime"];

		if (!hp_icon.is_valid())
			hp_icon = nl::nx::map001["Custom"]["IconHp"];

		if (!mp_icon.is_valid())
			mp_icon = nl::nx::map001["Custom"]["IconMp"];

		// THE POTIONS, IN THE BOTTOM CORNERS.
		//
		// At the FOOT of each gauge, not the head: the bars fill from the
		// bottom, so the potion sits where the liquid is and the corner it is
		// in says which bar it belongs to without a line being drawn.
		//
		// Bigger than the bar is wide, deliberately: a potion squeezed into 12
		// pixels is a coloured smudge.
		constexpr int16_t PIP = 22;

		auto pip = [this](const Texture& art, int16_t left, int16_t top)
		{
			draw_art(art, Rectangle<int16_t>(
				Point<int16_t>(left, top),
				Point<int16_t>(static_cast<int16_t>(left + PIP),
					static_cast<int16_t>(top + PIP))), 0);
		};

		int16_t pip_y = static_cast<int16_t>(Hh - VITAL_W - PIP);

		pip(hp_icon, -3, pip_y);
		pip(mp_icon, static_cast<int16_t>(W - PIP + 3), pip_y);

		// The clock, on the top strip, hard right - the left of that strip is
		// where the breadcrumb starts.
		{
			std::time_t now = std::time(nullptr);
			std::tm local {};

#ifdef _WIN32
			localtime_s(&local, &now);
#else
			localtime_r(&now, &local);
#endif

			char when[8];
			std::strftime(when, sizeof(when), "%H:%M", &local);

			// A size up, and clear of the gauge down the right edge.
			if (clock_text.get_text().empty())
				clock_text = Text(Text::Font::A12M, Text::Alignment::RIGHT,
					Color::Name::WHITE);

			clock_text.change_text(when);
			clock_text.draw(Point<int16_t>(
				static_cast<int16_t>(W - VITAL_W - 4), CHROME_TOP));

			draw_art(clock_icon, Rectangle<int16_t>(
				Point<int16_t>(static_cast<int16_t>(W - VITAL_W - 4 - 46 - CRUMB),
					CHROME_TOP),
				Point<int16_t>(static_cast<int16_t>(W - VITAL_W - 4 - 46),
					static_cast<int16_t>(CHROME_TOP + CRUMB))), 0);
		}

		// THE EXPERIENCE BAR, ALONG THE BOTTOM.
		//
		// It was across the TOP, thirty pixels thick, sitting squarely on the
		// home button. Down here it covers nothing, it is the bar you are
		// walking toward rather than one you are defending, and it is the
		// brightest thing on the panel - the only gauge that only ever goes up.
		int16_t level = stats.get_stat(Maplestat::Id::LEVEL);
		float exp_ratio = 0.0f;

		if (level < ExpTable::LEVELCAP)
			exp_ratio = static_cast<float>(
				static_cast<double>(stats.get_exp()) / ExpTable::values[level]);

		int16_t exp_y = static_cast<int16_t>(Hh - VITAL_W);

		// A slow shimmer, always. Nothing here is urgent, so it breathes
		// rather than flashes - but it breathes brighter than either potion.
		float shine = 0.86f + 0.14f * (std::sin(pulse * 0.030f) * 0.5f + 0.5f);

		channel(bar_h, 0, exp_y, W, VITAL_W);
		bar(RIM, static_cast<int16_t>(exp_y + RIM),
			static_cast<int16_t>((W - RIM * 2) * exp_ratio), VITAL_W - RIM * 2,
			1.00f * shine, 0.86f * shine, 0.42f * shine, 0.95f);

		// THE NUMBERS, ALL THREE ON ONE LINE, DIRECTLY ABOVE THE EXP BAR.
		//
		// Together rather than each at the foot of its own gauge: three
		// figures on a line are read in one glance, and three scattered round
		// the edge of a screen are three separate glances.
		int16_t num_y = static_cast<int16_t>(exp_y - 17);

		hp_text.change_text(std::to_string(hp) + "/" + std::to_string(maxhp));
		mp_text.change_text(std::to_string(mp) + "/" + std::to_string(maxmp));

		// Clear of the potions, which now occupy both bottom corners.
		hp_text.draw(Point<int16_t>(static_cast<int16_t>(PIP + 4), num_y));
		mp_text.draw(Point<int16_t>(static_cast<int16_t>(W - PIP - 4), num_y));

		char buf[16];
		std::snprintf(buf, sizeof(buf), "%.2f%%", exp_ratio * 100.0f);

		exp_text.change_text(buf);
		exp_text.draw(Point<int16_t>(static_cast<int16_t>(W / 2), num_y));

		// THE WASH, LAST, OVER EVERYTHING.
		//
		// Drawn after the page rather than before it, so the colour goes over
		// the backdrop and the icons instead of hiding behind them - which is
		// why the home screen never appeared to pulse.
		auto wash = [&](float ratio, float speed, float r, float g, float b)
		{
			if (ratio >= 0.35f)
				return;

			float urgency = (0.35f - ratio) / 0.35f;
			float wave = std::sin(pulse * speed) * 0.5f + 0.5f;

			GraphicsGL::get().drawrectangle(0, 0, screen.x(), screen.y(),
				r, g, b, urgency * wave * 0.30f);
		};

		wash(hp_ratio, 0.085f, 1.0f, 0.10f, 0.10f);
		wash(mp_ratio, 0.052f, 0.15f, 0.35f, 1.0f);
	}

	void SecondScreenPanel::channel(const Texture& art,
		int16_t x, int16_t y, int16_t w, int16_t h) const
	{
		if (w <= 0 || h <= 0)
			return;

		// No artwork - fall back to the drawn trough rather than nothing, so
		// a missing bitmap costs the look and not the gauge.
		if (!art.is_valid())
		{
			bar(x, y, w, h, 0.20f, 0.16f, 0.13f, 0.62f);
			return;
		}

		Point<int16_t> at(x, y);

		art.draw(DrawArgument(at, at, Point<int16_t>(w, h),
			1.0f, 1.0f, 1.0f, 0.0f));
	}

	void SecondScreenPanel::bar(int16_t x, int16_t y, int16_t w, int16_t h,
		float r, float g, float b, float a) const
	{
		// ROUNDED, by drawing the body and then capping it inset.
		//
		// There is no rounded-rectangle call in GraphicsGL and adding one
		// would mean a shader; three rectangles get the same read at this
		// size. A 2px bite out of each corner is all the eye needs to stop
		// calling it a box.
		if (w <= 0 || h <= 0)
			return;

		constexpr int16_t R = 2;

		if (w <= R * 2 || h <= R * 2)
		{
			GraphicsGL::get().drawrectangle(x, y, w, h, r, g, b, a);
			return;
		}

		// The body, full width, short of the ends.
		GraphicsGL::get().drawrectangle(x, y + R, w, h - R * 2, r, g, b, a);

		// The two caps, inset so the corners are bitten off.
		GraphicsGL::get().drawrectangle(x + R, y, w - R * 2, R, r, g, b, a);
		GraphicsGL::get().drawrectangle(x + R, y + h - R, w - R * 2, R, r, g, b, a);
	}

	void SecondScreenPanel::draw_art(const Texture& art, Rectangle<int16_t> box,
		int16_t pad, int16_t lift) const
	{
		if (!art.is_valid())
			return;

		Point<int16_t> full = art.get_dimensions();

		int16_t room_w = static_cast<int16_t>(box.width() - pad * 2);
		int16_t room_h = static_cast<int16_t>(box.height() - pad * 2);

		if (full.x() <= 0 || full.y() <= 0 || room_w <= 0 || room_h <= 0)
			return;

		// Keep the shape. A mushroom house squashed into a square stops being
		// a mushroom house.
		float scale = std::min(
			static_cast<float>(room_w) / full.x(),
			static_cast<float>(room_h) / full.y());

		// Never blow a 30px icon up to fill a tile; it only goes soft.
		if (scale > 1.6f)
			scale = 1.6f;

		Point<int16_t> size(
			static_cast<int16_t>(full.x() * scale),
			static_cast<int16_t>(full.y() * scale));

		Point<int16_t> at(
			static_cast<int16_t>(box.left() + (box.width() - size.x()) / 2),
			static_cast<int16_t>(box.top() + (box.height() - size.y()) / 2 - lift));

		// The origin, added back - see the header.
		Point<int16_t> p = at + art.get_origin();

		art.draw(DrawArgument(p, p, size, 1.0f, 1.0f, 1.0f, 0.0f));
	}

	Rectangle<int16_t> SecondScreenPanel::home_box(Point<int16_t> screen) const
	{
		constexpr int16_t S = 26;

		// BELOW the top strip. It used to sit at y=2, which is where the EXP
		// bar was before it moved to the bottom - the bar was drawn straight
		// over it and the way home was invisible.
		constexpr int16_t TOP = VITAL_W + 3;

		return Rectangle<int16_t>(
			Point<int16_t>(screen.x() - VITAL_W - S - 4, TOP),
			Point<int16_t>(screen.x() - VITAL_W - 4, TOP + S));
	}

	void SecondScreenPanel::draw_menu(Point<int16_t> screen) const
	{
		// NO FRAME HERE - see draw_frame, called before the page.
		//
		// It WAS drawn here, and draw_menu runs at the end of draw_chrome,
		// which runs after the page's window. So a full-panel sheet was being
		// painted straight over the map, the inventory and the stats: every
		// leaf page went blank the moment the frame went on every page. A
		// background has to be drawn like one.
		//
		// The way home is the leftmost breadcrumb now, not a mark in the far
		// corner - see crumb_box.
		size_t count = 0;
		const Page* items = menu_items(current, count);

		if (!items)
			return;

		if (menu_label.get_text().empty())
			menu_label = Text(Text::Font::A11M, Text::Alignment::CENTER,
				Color::Name::WHITE);

		for (size_t i = 0; i < count; i++)
		{
			Rectangle<int16_t> box = menu_box(i, count, screen);

			// Lifted, so the label has the lower strip to itself.
			draw_art(page_art(items[i]), box, 10, 12);

			// THE NAME, UNDER THE PICTURE.
			//
			// The icons went label-less once they were in the breadcrumb, on
			// the theory that the trail teaches them. It teaches the ones you
			// have already pressed; it cannot teach a button you have never
			// opened, which is every button the first time.
			if (const char* name = page_name(items[i]))
			{
				menu_label.change_text(name);
				menu_label.draw(Point<int16_t>(
					static_cast<int16_t>(box.left() + box.width() / 2),
					static_cast<int16_t>(box.bottom() - 20)));
			}
		}
	}

	void SecondScreenPanel::draw_options(Point<int16_t> screen) const
	{
		// NAMES AND VALUES, NO CONTROLS YET.
		//
		// The real settings, read out of Configuration rather than invented,
		// so the list cannot claim an option the game does not have. Rows are
		// the same faint cell the inventory, equipment and skill grids draw,
		// which is what makes this look like a page of this panel.
		//
		// They are not editable from here yet and there are no icons - the
		// artwork has not been chosen, and a row of placeholders would be
		// harder to replace later than a row of words.
		struct Row { const char* name; std::string value; };

		// Setting<T>::get().load() is how the rest of the client reads these -
		// there are no plain getters on Configuration for them.
		const Row rows[] = {
			{ "Music",      std::to_string(Setting<BGMVolume>::get().load()) },
			{ "Sound",      std::to_string(Setting<SFXVolume>::get().load()) },
			{ "Save ID",    Setting<SaveLogin>::get().load() ? "on" : "off" },
			{ "Fullscreen", Setting<Fullscreen>::get().load() ? "on" : "off" },
			{ "Resolution", std::to_string(Setting<Width>::get().load())
				+ " x " + std::to_string(Setting<Height>::get().load()) },
			{ "Server",     Setting<ServerIP>::get().load() },
		};

		if (option_text.get_text().empty())
			option_text = Text(Text::Font::A12M, Text::Alignment::LEFT,
				Color::Name::WHITE);

		constexpr int16_t ROW_H = 30;
		constexpr int16_t TOP = 34;

		int16_t left = static_cast<int16_t>(VITAL_W + 6);
		int16_t width = static_cast<int16_t>(screen.x() - left * 2);

		for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
		{
			int16_t y = static_cast<int16_t>(TOP + i * ROW_H);

			GraphicsGL::get().drawrectangle(
				left, y, width, ROW_H - 4, 1.0f, 1.0f, 1.0f, 0.14f);

			option_text.change_text(rows[i].name);
			option_text.draw(Point<int16_t>(static_cast<int16_t>(left + 8), y + 3));

			option_text.change_text(rows[i].value);
			option_text.draw(Point<int16_t>(
				static_cast<int16_t>(left + width - 90), y + 3));
		}
	}

	Rectangle<int16_t> SecondScreenPanel::talk_box(Point<int16_t> screen) const
	{
		// Most of the page, low down, where a thumb already is.
		constexpr int16_t TOP = 60;

		int16_t left = static_cast<int16_t>(VITAL_W + 10);

		return Rectangle<int16_t>(
			Point<int16_t>(left, TOP),
			Point<int16_t>(static_cast<int16_t>(screen.x() - left),
				static_cast<int16_t>(screen.y() - VITAL_W - 14)));
	}

	void SecondScreenPanel::draw_voice(Point<int16_t> screen) const
	{
		Voice& voice = Voice::get();

		if (voice_text.get_text().empty())
		{
			voice_text = Text(Text::Font::A13M, Text::Alignment::CENTER,
				Color::Name::WHITE);
			voice_hint = Text(Text::Font::A11M, Text::Alignment::CENTER,
				Color::Name::WHITE);
		}

		Rectangle<int16_t> box = talk_box(screen);
		bool live = voice.is_talking();

		// RED WHILE LIVE, and unmistakably so. A microphone that is on and
		// does not look on is the one failure mode worth designing against.
		float wave = std::sin(pulse * 0.10f) * 0.5f + 0.5f;

		GraphicsGL::get().drawrectangle(
			box.left(), box.top(), box.width(), box.height(),
			live ? 0.60f + 0.35f * wave : 0.16f,
			live ? 0.14f : 0.18f,
			live ? 0.14f : 0.16f,
			live ? 0.95f : 0.45f);

		if (!voice.is_open())
		{
			voice_text.change_text("VOICE OFF");
			voice_hint.change_text("Tap to switch on");
		}
		else if (live)
		{
			voice_text.change_text("TALKING");
			voice_hint.change_text("Let go to stop");
		}
		else
		{
			voice_text.change_text("HOLD TO TALK");
			voice_hint.change_text("Everyone on this wifi hears you");
		}

		int16_t mid = static_cast<int16_t>(box.left() + box.width() / 2);

		voice_text.draw(Point<int16_t>(mid,
			static_cast<int16_t>(box.top() + box.height() / 2 - 22)));
		voice_hint.draw(Point<int16_t>(mid,
			static_cast<int16_t>(box.top() + box.height() / 2 + 4)));
	}

	void SecondScreenPanel::draw_frame(Point<int16_t> screen) const
	{
		// WHEREVER THE PARTY IS STANDING.
		//
		// The panel wears the current map's own background - the sky and the
		// far scenery, drawn from the same camera the top screen uses, so it
		// drifts as they walk and changes the moment they take a portal. A
		// fixed wooden frame said "this is a menu"; this says "this is the
		// same world, seen from your hands".
		//
		// Backgrounds only. No tiles, objects or mobs - a second copy of the
		// fight underneath the inventory would be unreadable, and the parallax
		// layers are the part that carries the place.
		//
		// Dimmed, because everything else on this panel has to stay legible on
		// top of it. Some maps are bright noon skies and some are caves.
		if (Stage::get().is_active())
		{
			Point<double> view = Stage::get().view_position(1.0f);

			Stage::get().draw_backdrop(view.x(), view.y(), 1.0f);

			GraphicsGL::get().drawrectangle(0, 0, screen.x(), screen.y(),
				0.04f, 0.03f, 0.06f, BACKDROP_DIM);

			return;
		}

		// AT THE LOGIN SCREEN there is no map to borrow, so the old frame is
		// still the fallback. The login and quit windows are built on it, so
		// the panel belongs to the same game before a character exists.
		if (!tile_frame.is_valid())
			return;

		Point<int16_t> at(0, 0);

		tile_frame.draw(DrawArgument(at, at, screen, 1.0f, 1.0f, 1.0f, 0.0f));
	}

	size_t SecondScreenPanel::crumb_count() const
	{
		// The way we came, plus where we are.
		return trail.size() + 1;
	}

	SecondScreenPanel::Page SecondScreenPanel::crumb_page(size_t index) const
	{
		return index < trail.size() ? trail[index] : current;
	}

	Rectangle<int16_t> SecondScreenPanel::crumb_box(size_t index) const
	{
		// TOP LEFT, LEVEL WITH THE CLOCK, CLEAR OF THE GAUGE.
		//
		// The address bar and the time are the two things that are true
		// wherever you are, so they share one line - one at each end of it.
		// It starts inboard of VITAL_W because the HP gauge runs the full
		// height of the left edge now and home was sitting on top of it.
		constexpr int16_t GAP = 4;

		int16_t x = static_cast<int16_t>(VITAL_W + 4 + index * (CRUMB + GAP));

		return Rectangle<int16_t>(
			Point<int16_t>(x, CHROME_TOP),
			Point<int16_t>(static_cast<int16_t>(x + CRUMB),
				static_cast<int16_t>(CHROME_TOP + CRUMB)));
	}

	Texture SecondScreenPanel::page_art(Page page) const
	{
		auto found = art_cache.find(page);

		if (found != art_cache.end())
			return found->second;

		// HAND-PICKED ARTWORK, built into Map001.nx by make_assets.py.
		//
		// Not nodes borrowed from the game's own files: a town's map mark is
		// not an inventory, and picking art by reading node names put a
		// monster on Character and a picture of Perion on Equipment.
		nl::node custom = nl::nx::map001["Custom"];

		Texture art;

		switch (page)
		{
		case HOME:      art = custom["IconHome"]; break;
		case CHARACTER: art = custom["IconCharacter"]; break;
		case ADVENTURE: art = custom["IconAdventure"]; break;
		case INVENTORY: art = custom["IconInventory"]; break;
		case EQUIPMENT: art = custom["IconEquipment"]; break;
		case ABILITY:   art = custom["IconStats"]; break;
		case QUESTS:    art = custom["IconQuest"]; break;
		case HOTKEYS:   art = custom["IconHotkeys"]; break;
		case CHAT:      art = custom["IconSocial"]; break;
		case WORLDMAP:  art = custom["IconMap"]; break;
		case SETTINGS:  art = custom["IconSettings"]; break;
		case MINIGAME:  art = custom["IconMinigame"]; break;
		case SHOUT:     art = custom["IconShout"]; break;
		case ROOMCHAT:  art = custom["IconRoomMessage"]; break;
		default: break;
		}

		art_cache[page] = art;

		return art;
	}

	const char* SecondScreenPanel::page_name(Page page)
	{
		switch (page)
		{
		case HOME:      return "Home";
		case ADVENTURE: return "Adventure";
		case CHARACTER: return "Character";
		case INVENTORY: return "Inventory";
		case EQUIPMENT: return "Equipment";
		case ABILITY:   return "Stats";
		case QUESTS:    return "Quests";
		case HOTKEYS:   return "Hotkeys";
		case CHAT:      return "Social";
		case SETTINGS:  return "Settings";
		case MINIGAME:  return "Minigames";
		case SHOUT:     return "Voice";
		case ROOMCHAT:  return "Messages";
		default:        return nullptr;
		}
	}

	bool SecondScreenPanel::hotkey_jump_visible() const
	{
		// Only where there is something to carry away.
		return current == INVENTORY || current == EQUIPMENT || current == ABILITY;
	}

	Rectangle<int16_t> SecondScreenPanel::hotkey_jump_box(Point<int16_t> screen) const
	{
		// Bigger than a mouse button wants to be: this is a thumb, on a panel
		// held at arm's length.
		// Against a 344x300 panel, not the backdrop bitmap's 620x540.
		constexpr int16_t W = 104;
		constexpr int16_t H = 28;

		// BOTTOM right. It was at the top, beside the page title, which is
		// where a mouse would look for it and the furthest point from where a
		// thumb already rests.
		return Rectangle<int16_t>(
			Point<int16_t>(screen.x() - W - 10, screen.y() - H - 10),
			Point<int16_t>(screen.x() - 10, screen.y() - 10));
	}

	void SecondScreenPanel::draw_chrome(Point<int16_t> screen) const
	{
		// Which page this is, across the top. Not on the map: it names itself
		// and wants every pixel.
		// WHERE YOU ARE, not just what you are looking at.
		//
		// "Menu > Character > Inventory" - the trail you took to get here, so
		// the way out is obvious without a Back button to find.
		{
			// EACH STEP CARRIES ITS OWN ICON, AND EACH IS A BUTTON.
			//
			// Top left, home first, because home is the root and every trail
			// starts there. Pressing any of them goes back to that folder -
			// which makes the address bar the way out as well as the sign
			// saying where you are, and retires the separate home mark that
			// used to sit in the opposite corner from everything else.
			for (size_t i = 0; i < crumb_count(); i++)
			{
				Texture art = page_art(crumb_page(i));

				if (art.is_valid())
					draw_art(art, crumb_box(i), 0);
			}

			// AND THE NAME OF WHERE YOU ARE, after the last icon.
			//
			// Icons alone were fine going in - you had just pressed the
			// button, so you knew what it meant - but they say nothing about
			// a page you arrived at by a swipe, and nothing at all the first
			// time. Only the LAST step is named: the trail behind it stays a
			// compact row of pictures, which is what it is for.
			if (const char* here = page_name(current))
			{
				if (crumb_label.get_text().empty())
					crumb_label = Text(Text::Font::A12M, Text::Alignment::LEFT,
						Color::Name::WHITE);

				Rectangle<int16_t> last = crumb_box(crumb_count() - 1);

				crumb_label.change_text(here);
				crumb_label.draw(Point<int16_t>(
					static_cast<int16_t>(last.right() + 6), CHROME_TOP + 2));
			}
		}

		// A way STRAIGHT TO THE HOTKEYS from the pages you fill them from.
		//
		// Drawn here rather than added to the inventory, equipment and skill
		// windows: those are the game's own windows, shared with the main
		// screen, and each would need its own button, artwork and hit test.
		// One button in the panel's chrome appears on exactly the pages that
		// want it and cannot break a window that works.
		if (!has_guest() && hotkey_jump_visible())
		{
			Rectangle<int16_t> box = hotkey_jump_box(screen);

			GraphicsGL::get().drawrectangle(
				box.left(), box.top(), box.width(), box.height(),
				0.10f, 0.11f, 0.09f, 0.85f);

			if (hotkey_jump.get_text().empty())
				hotkey_jump = Text(Text::Font::A11B, Text::Alignment::CENTER,
					Color::Name::WHITE, "TO HOTKEYS");

			hotkey_jump.draw(Point<int16_t>(
				box.left() + box.width() / 2, box.top() + 6));
		}

		// No page dots and no arrows. Both belonged to a deck you swiped
		// through; this is a tree you press into and swipe out of, and a row
		// of dots would be counting something that no longer exists.
		draw_menu(screen);
		draw_vitals(screen);

		// A mark at each side saying there is more that way. Small, yellow and
		// out of the way - the page itself is what matters.
		// Half the artwork's size. At full size on a panel laid out this large
		// they were a third of the screen apart and covered the map.
		//
		// Drawn from the top-left corner, so half the drawn size comes off to
		// sit them on the middle of the side.
		Point<int16_t> full = arrow_left.get_dimensions();
		Point<int16_t> size = Point<int16_t>(full.x() / 2, full.y() / 2);

		int16_t right = screen.x() - ARROW_INSET - size.x();

		Point<int16_t> at_left = Point<int16_t>(ARROW_INSET, ARROW_INSET);
		Point<int16_t> at_right = Point<int16_t>(right, ARROW_INSET);

		// NOT DRAWN. Page turning is a swipe now, and an arrow is a thing to
		// aim at that no longer does anything. Kept only so the geometry above
		// still compiles for anything that asks.
		(void)at_left;
		(void)at_right;
		(void)size;
	}

	const Texture* SecondScreenPanel::page_backdrop() const
	{
		// A page may bring its own backdrop instead of the panel's forest. The
		// inventory does: it is a bag, and it reads as one.
		// Each page may bring its own. The inventory is a bag and reads as one;
		// the equipment page is the rack the gear hangs on.
		if (guest && guest_backdrop_name)
		{
			if (!guest_backdrop_tex.is_valid())
				guest_backdrop_tex = nl::nx::map001["Custom"][guest_backdrop_name];

			return guest_backdrop_tex.is_valid() ? &guest_backdrop_tex : nullptr;
		}

		// NO PER-PAGE BACKDROP ON OUR OWN TABS.
		//
		// Inventory, equipment, stats, social and hotkeys each carried their
		// own full-bleed picture - a bag, a rack, a desk. Five pages, five
		// different rooms, and the panel stopped reading as one place: the
		// frame under them is the thing that makes them a set, and a photo
		// over it hid exactly that.
		//
		// A SHOP still brings its own. A shop is a guest, not one of our tabs
		// - it arrives because an NPC opened it, and looking unlike the rest
		// of the panel is correct for something that is only passing through.
		return nullptr;
	}

	void SecondScreenPanel::draw(Point<int16_t> screen) const
	{
		panel_screen = screen;

		// FIRST, under everything. See draw_frame.
		draw_frame(screen);

		// Behind the page, and dimmed - it is a background, and the slots and
		// item icons on top of it have to stay readable. BACKDROP_FADE is the
		// whole of that: turn it up to bring the bag forward.
		if (const Texture* backdrop = page_backdrop())
			backdrop->draw(DrawArgument(Point<int16_t>(0, 0), Point<int16_t>(0, 0),
				screen, 1.0f, 1.0f, BACKDROP_FADE, 0.0f));

		UIElement* element = window();

		if (!Stage::get().is_active())
		{
			// The wordmark rather than a line of text.
			if (!logo_tried)
			{
				logo_tried = true;
				logo = nl::nx::map001["Custom"]["DsLogo"];
			}

			if (logo.is_valid())
			{
				Point<int16_t> size = logo.get_dimensions();

				logo.draw(Point<int16_t>(
					(screen.x() - size.x()) / 2, 20));
			}

			return;
		}

		if (element)
		{
			Point<int16_t> at = window_position(screen);

			element->set_position(at);
			element->draw(1.0f);
		}
		else if (current == SETTINGS)
		{
			draw_options(screen);
		}
		else if (current == SHOUT)
		{
			draw_voice(screen);
		}
		else
		{
			// A page with nothing behind it yet still shows its own space, so
			// arriving there reads as arriving somewhere rather than as the
			// panel having broken.
			//
			// FAINT, not the 0.35 black slab it was - that was the same dark
			// plate the quest log and the stat sheet have just had taken off
			// them, and leaving it here would make the empty pages the odd
			// ones out instead.
			GraphicsGL::get().drawrectangle(
				VITAL_W + 4, CONTENT_TOP + 30,
				screen.x() - (VITAL_W + 4) * 2, screen.y() - CONTENT_TOP - 30 - VITAL_W - 4,
				1.0f, 1.0f, 1.0f, 0.10f);
		}

		draw_chrome(screen);

		// Over everything, including the arrows and the dots - for five seconds
		// the panel is a level-up and nothing else. Centred rather than
		// stretched: the artwork is square and the panel is not.
		if (levelup_playing)
		{
			Point<int16_t> size = levelup.get_dimensions();

			if (size.x() > 0 && size.y() > 0)
			{
				// Scaled until it COVERS the panel rather than fits inside it,
				// so there are no bands down the sides. The artwork is square
				// and the panel is not, so covering crops a little off the top
				// and bottom - which is better than stretching a burst of
				// light into an oval.
				float across = static_cast<float>(screen.x()) / size.x();
				float down = static_cast<float>(screen.y()) / size.y();
				float scale = across > down ? across : down;

				Point<int16_t> to(
					static_cast<int16_t>(size.x() * scale),
					static_cast<int16_t>(size.y() * scale));

				Point<int16_t> at(
					(screen.x() - to.x()) / 2,
					(screen.y() - to.y()) / 2);

				levelup.draw(DrawArgument(at, to), 1.0f);
			}
		}

		// Over everything else, because it is the thing being aimed with.
		//
		// There is one cursor between the two screens. If the main screen has
		// taken it back - it does that the moment it is touched - then it is
		// not here any more.
		// The hover box is NOT drawn here - see draw_top_tooltip. Reading it
		// meant looking down at the small screen while the thing it describes
		// is on the big one, and it covered a third of the map besides.
		// NO CURSOR DOWN HERE. A cursor is for a pointer you cannot see; a
		// finger is already on the glass, and an arrow trailing it is one more
		// thing on a screen that wants fewer.
	}
}
