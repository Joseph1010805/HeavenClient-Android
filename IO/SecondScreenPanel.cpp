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
#include "../Util/GiftBox.h"
#include "../Util/PostBox.h"

#include "../Net/Packets/DueyPackets.h"
#include "UITypes/UITrade.h"
#include "UITypes/UIStorage.h"

#include "../Net/Packets/TradePackets.h"
#include "UITypes/UICharacterPage.h"

#include "UI.h"
#include "Window.h"
#include <map>
#include "Keyboard.h"
#include "KeyAction.h"

#include "UITypes/UIEquipInventory.h"
#include "UITypes/UIItemInventory.h"
#include "UITypes/UIQuestLog.h"
#include "UITypes/UISkillbook.h"
#include "UITypes/UIStatsinfo.h"
#include "UITypes/UIWorldMap.h"
#include "UITypes/UIMiniMap.h"
#include "UITypes/UIQuit.h"
#include "UITypes/UIKeyConfig.h"
#include "UITypes/UINotice.h"
#include "UITypes/UIChatbar.h"

#include "../Configuration.h"
#include "../Net/Packets/MessagingPackets.h"
#include "../Character/Party.h"
#include "../Net/Packets/GameplayPackets.h"
#include "../Speech.h"
#include "../Voice.h"
#include "../Constants.h"
#include "../Gameplay/Stage.h"
#include "../Character/ExpTable.h"
#include "../Data/ItemData.h"
#include "../Character/Look/Face.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <deque>
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

	// WHAT COUNTS AS A DELIBERATE SWIPE ON A MAP.
	//
	// Further than an ordinary page swipe and quick with it: 25 frames at the
	// client's 125-a-second step is a fifth of a second, which is a flick
	// rather than a drag. Somebody reading the map moves further than this,
	// but never this fast.
	constexpr int16_t MAP_SWIPE_MIN = 130;
	constexpr uint16_t MAP_SWIPE_TICKS = 25;

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

		// THE SEVEN FACES v83 ACTUALLY HAS.
		//
		// Face ids 0-6 in the order the game's own emotion window shows them.
		// Cosmic reads FACE_EXPRESSION as a single int and validates it, so
		// there is nothing to gain by offering ids that do not exist.
		// NOT `Face` - that is the game's own class, which this page now
		// draws with. One of them had to have a different name.
		// The action each emote is bound to, so the page can borrow the key
		// config's picture for it - the same seven the keyboard window shows.
		struct Emote { const char* name; int32_t id; KeyAction::Id key; };

		// THE REAL LIST, and where the first attempt went wrong.
		//
		// I invented seven names against seven ids and got both wrong. The
		// game has TWENTY-FOUR expressions - Expression::Id in Face.h - and
		// the server decides which of them are free:
		//
		//   FaceExpressionHandler rejects `emote < 1` outright, allows 1-7
		//   with no check at all, and for anything above 7 requires you to
		//   OWN the matching cash emote item (5159992 + emote, ids 5160000
		//   to 5160014).
		//
		// So 1-7 are the free set, "Default" is not sendable at all, and the
		// other sixteen are items you have to buy. These are the client's own
		// Expression::names, so the label and the face agree by construction
		// rather than by my guessing at them.
		const Emote FACES[] = {
			{ "Blink",      Expression::Id::BLINK,      KeyAction::Id::FACE1 },
			{ "Hit",        Expression::Id::HIT,        KeyAction::Id::FACE2 },
			{ "Smile",      Expression::Id::SMILE,      KeyAction::Id::FACE3 },
			{ "Troubled",   Expression::Id::TROUBLED,   KeyAction::Id::FACE4 },
			{ "Cry",        Expression::Id::CRY,        KeyAction::Id::FACE5 },
			{ "Angry",      Expression::Id::ANGRY,      KeyAction::Id::FACE6 },
			{ "Bewildered", Expression::Id::BEWILDERED, KeyAction::Id::FACE7 },
		};

		// Above this the server wants the emote ITEM in your inventory.
		constexpr int32_t FREE_EMOTE_MAX = 7;

		constexpr size_t FACE_COUNT = sizeof(FACES) / sizeof(FACES[0]);

		// THE INFO QUEST THE DAILY COUNT ARRIVES IN.
		//
		// Must match DailyPve.PROGRESS_QUEST on the server. 7770 is outside
		// v83's quest range, so it collides with nothing real - and the
		// journal skips it, or the hunt would show up as a quest you cannot
		// hand in.
		constexpr int16_t DAILY_QUEST = 7770;

		// The three-minute rush publishes "secondsLeft:kills" through its own
		// info quest. Same trick, next number along - see DailyRush.java.
		constexpr int16_t RUSH_QUEST = 7771;

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
			|| page == CHAT || page == SETTINGS || page == DAILY
			|| page == INVENTORY || page == EQUIPMENT;
	}

	const SecondScreenPanel::Page* SecondScreenPanel::menu_items(Page page, size_t& count)
	{
		// SIX, in three rows of two.
		static const Page home[] = {
			CHARACTER, ADVENTURE, HOTKEYS, CHAT, SETTINGS, CASHSHOP
		};

		// WHO you are: the three pages about the person.
		//
		// Quests left here for ADVENTURE. A quest is not a fact about your
		// character, it is somewhere you are going - and it sat next to your
		// stats only because there were four tabs on the top screen.
		static const Page character[] = { INVENTORY, EQUIPMENT, ABILITY, SKILLS };

		// WHERE you are going: the map and the quest log answer the same
		// question from two directions.
		// WHERE you are going: the map, the quest log - and the dailies, which
		// are the same question asked about today rather than about the story.
		static const Page adventure[] = { MINIMAP, WORLDMAP, QUESTS, DAILY };

		// TALKING: saying something, reading what was said, and the two
		// things that are not words.
		//
		// MAIL AND THE MEGAPHONE ARE BOTH GONE.
		//
		// Mail was a second inbox beside the chat log, addressed by picking a
		// name off a list - a whole page and a keyboard for something the
		// running chat already does. The post box UNDER it is still there and
		// still delivers: a line sent to somebody who is not connected waits
		// and arrives in their chat when they next log in, from across the
		// world if a relay is set. It simply has no page of its own now.
		//
		// The megaphone became L3. A stick click reaches the whole world as a
		// banner, which is what that page was for and is one press instead of
		// four.
		static const Page social[] = { SAY, ROOMCHAT, NEARBY, GIFT, EMOTIONS, PARTY };

		// The list of options, the pad, and a way to tell us it broke.
		// OPTIONS IS GONE. It was a read-only list of what Configuration
		// happened to hold - nothing on it could be changed, and nothing on
		// it is a decision a player of this build has to make. The other four
		// all are: what the keys do, what the pad does, telling us it broke,
		// and leaving.
		static const Page settings[] = {
			KEYBINDS, CONTROLLER, REPORT, EXITGAME
		};

		// What you come back for each day.
		static const Page daily[] = { PVE, PVP };

		// The bag's five sections.
		static const Page bag[] = {
			INV_EQUIP, INV_USE, INV_SETUP, INV_ETC, INV_CASH
		};

		// WHAT IS ON YOU, in the same three parts the window used to put in a
		// tab strip: the gear itself, the cosmetics worn over it, and the pet.
		static const Page worn[] = { EQ_GEAR, EQ_CASH, EQ_PET };

		switch (page)
		{
		case HOME:
			count = sizeof(home) / sizeof(home[0]);
			return home;
		case EQUIPMENT:
			count = sizeof(worn) / sizeof(worn[0]);
			return worn;
		case CHARACTER:
			count = sizeof(character) / sizeof(character[0]);
			return character;
		case ADVENTURE:
			count = sizeof(adventure) / sizeof(adventure[0]);
			return adventure;
		case CHAT:
			count = sizeof(social) / sizeof(social[0]);
			return social;
		case SETTINGS:
			count = sizeof(settings) / sizeof(settings[0]);
			return settings;
		case DAILY:
			count = sizeof(daily) / sizeof(daily[0]);
			return daily;
		case INVENTORY:
			count = sizeof(bag) / sizeof(bag[0]);
			return bag;
		default:
			count = 0;
			return nullptr;
		}
	}

	void SecondScreenPanel::go_to(Page which)
	{
		// ASK EVERY TIME IT IS OPENED. The server owns the counter and this
		// client keeps no copy worth trusting - a list from ten minutes ago
		// would offer a parcel that has already been collected.
		if (which == GIFT)
		{
			GiftBox::get().clear_result();

			if (Stage::get().is_active())
				DueyOpenPacket().dispatch();
		}

		if (which < 0 || which >= NUM_PAGES || which == current)
			return;

		trail.push_back(current);
		current = which;

		leave_page();

		// EXCEPT WHEN THE DESTINATION IS THE HOTKEY PAGE.
		//
		// Turning a page normally means letting go of whatever was picked
		// out - a potion selected on the item page has no business still
		// being held three screens later.
		//
		// But the TO HOTKEYS button navigates, and carrying the thing there
		// is the ENTIRE point of it: it cleared the selection on the way to
		// the one page that exists to receive it. Pressing the button worked,
		// arrived, and put you down empty-handed. Same for a skill.
		if (which != HOTKEYS)
			carried = Keyboard::Mapping();
	}

	bool SecondScreenPanel::take_back_at_root()
	{
		bool asked = back_at_root;

		back_at_root = false;

		return asked;
	}

	void SecondScreenPanel::go_back()
	{
		if (trail.empty())
		{
			back_at_root = true;

			return;
		}

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

	UIElement* SecondScreenPanel::open_trade()
	{
		show_page(TRADE);

		// window() is what BUILDS a page the first time it is looked at, and
		// the first thing that happens in a trade is a packet arriving before
		// anybody has looked at anything. Asking hosted() instead would find
		// nothing and drop the opening move.
		return window();
	}

	UIElement* SecondScreenPanel::open_storage()
	{
		show_page(STORAGE);

		return window();
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
		// THE PAGES THE PANEL DRAWS ITSELF HAVE NO WINDOW TO ASK.
		//
		// This handed everything to window(), which is null on every page
		// this class paints - so the stick moved the inventory and the skill
		// list and did nothing whatever on the keys.
		if (current == KEYBINDS)
		{
			key_scroll = static_cast<int16_t>(key_scroll - yoffset * 2);

			if (key_scroll < 0)
				key_scroll = 0;

			int16_t last = static_cast<int16_t>(
				bindable_count() > 4 ? bindable_count() - 4 : 0);

			if (key_scroll > last)
				key_scroll = last;

			return;
		}

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
		// The pages the panel draws itself. A menu is buttons; the rest are
		// pages this class paints rather than windows it hosts.
		if (is_menu(current)
			|| current == OPTIONS || current == CONTROLLER || current == REPORT
			|| current == KEYBINDS || current == EXITGAME || current == PARTY
			|| current == PVE || current == PVP
			|| current == SHOUT || current == ROOMCHAT || current == EMOTIONS
			|| current == SAY || current == NEARBY || current == GIFT)
			return nullptr;

		auto& slot = const_cast<std::unique_ptr<UIElement>&>(pages[current]);

		if (slot)
			return slot.get();

		// Built on first sight rather than up front, and not at all until a map
		// is loaded: most of these read the player, and at the login screen
		// there is no player to read.
		if (!Stage::get().is_active())
			return nullptr;

		// WHAT A PAGE IS GIVEN TO LAY ITSELF OUT IN.
		//
		// The content box, not the screen. Passing the screen is what put
		// every grid's last row and every action bar under the EXP gauge.
		Point<int16_t> room = Point<int16_t>(
			content_area(panel_screen).width(),
			content_area(panel_screen).height());

		switch (current)
		{
		case TRADE:
		{
			auto table = std::make_unique<UITrade>();

			table->set_panel(room);

			slot = std::move(table);
			break;
		}
		case STORAGE:
		{
			auto bank = std::make_unique<UIStorage>();

			bank->set_panel(room);

			slot = std::move(bank);
			break;
		}
		case MINIMAP:
		{
			auto mini = std::make_unique<UIMiniMap>(
				Stage::get().get_player().get_stats());

			mini->set_panel(room);
			mini->set_panel_tooltip(&tooltip);

			slot = std::move(mini);
			break;
		}
		case WORLDMAP:
		{
			auto map = std::make_unique<UIWorldMap>();
			map->set_panel(room);

			// Its own tooltip still - the two maps must not fight over the
			// shared one - but drawn on the MAIN screen, which is bigger and
			// is not the thing being covered up. See draw_top_tooltip.
			map->set_panel_tooltip(&tooltip);
			slot = std::move(map);
			break;
		}
		case INV_EQUIP:
		case INV_USE:
		case INV_SETUP:
		case INV_ETC:
		case INV_CASH:
		{
			auto bag = std::make_unique<UIItemInventory>(
				Stage::get().get_player().get_inventory());

			bag->set_panel(room);

			// The section this page IS. The window still knows how to change
			// tab - the quickslot bar and the shop both use it - but nothing
			// on the panel offers that any more, because the way between
			// sections is the panel's own buttons.
			bag->change_tab(
				current == INV_EQUIP ? InventoryType::Id::EQUIP :
				current == INV_USE   ? InventoryType::Id::USE :
				current == INV_SETUP ? InventoryType::Id::SETUP :
				current == INV_ETC   ? InventoryType::Id::ETC :
				InventoryType::Id::CASH);

			slot = std::move(bag);
			break;
		}
		case EQ_GEAR:
		case EQ_CASH:
		case EQ_PET:
		{
			auto worn = std::make_unique<UIEquipInventory>(
				Stage::get().get_player().get_inventory());

			worn->set_panel(room);

			// The section this page IS, exactly as the bag pages do it. The
			// window still knows how to change tab - nothing on the panel
			// offers that any more, because the way between sections is the
			// panel's own buttons.
			worn->show_section(
				current == EQ_GEAR ? UIEquipInventory::Section::GEAR :
				current == EQ_CASH ? UIEquipInventory::Section::COSMETIC :
				UIEquipInventory::Section::PET);

			slot = std::move(worn);
			break;
		}
		case ABILITY:
		{
			// THE STAT SHEET ALONE.
			//
			// The skills were merged in beneath it as one scrolling column,
			// and that column is why the skill page could never be laid out:
			// everything it drew was measured against a panel it did not own,
			// and its action bar spent three sessions below the screen. Two
			// pages, each the size of the panel, each able to say where its
			// own bottom is.
			auto sheet = std::make_unique<UIStatsinfo>(
				Stage::get().get_player().get_stats());

			sheet->set_panel(room);
			slot = std::move(sheet);
			break;
		}
		case SKILLS:
		{
			auto book = std::make_unique<UISkillbook>(
				Stage::get().get_player().get_stats(),
				Stage::get().get_player().get_skills());

			book->set_panel(room);
			slot = std::move(book);
			break;
		}
		case QUESTS:
		{
			auto log = std::make_unique<UIQuestLog>(
				Stage::get().get_player().get_quests());

			log->set_panel(room);
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

	Rectangle<int16_t> SecondScreenPanel::content_area(Point<int16_t> screen) const
	{
		// LEFT and RIGHT: clear of the two gauges, plus a hair so nothing
		// touches them.
		// TOP: under the address bar and the clock, which share one row.
		// BOTTOM: above the EXP bar AND the three numbers sitting on top of
		// it - the numbers are the part that gets forgotten, and they are
		// what a grid's last row disappears behind.
		constexpr int16_t SIDE = VITAL_W + 4;
		constexpr int16_t TOP = CHROME_TOP + CRUMB + 5;
		constexpr int16_t BOTTOM = VITAL_W + 22;

		return Rectangle<int16_t>(
			Point<int16_t>(SIDE, TOP),
			Point<int16_t>(static_cast<int16_t>(screen.x() - SIDE),
				static_cast<int16_t>(screen.y() - BOTTOM)));
	}

	Point<int16_t> SecondScreenPanel::window_position(Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		UIElement* element = window();

		if (!element)
			return Point<int16_t>(box.left(), box.top());

		Point<int16_t> size = element->get_dimension();

		// A page told to fill the content box gets its corner. Centring
		// something the size of the box only pushes it out of the box.
		if (size.x() >= box.width() && size.y() >= box.height())
			return Point<int16_t>(box.left(), box.top());

		int16_t spare = static_cast<int16_t>(box.height() - size.y());

		// A THIRD OF THE WAY DOWN, NOT HALF, ON THE MINIMAP.
		//
		// Dead centre put it low enough to read as if it had fallen to the
		// bottom of the page - the eye reads a lone object as centred when it
		// sits slightly high, not when it is measured equal. Only this page:
		// the windows are furniture with their own frames and look wrong
		// anywhere but the middle.
		if (current == MINIMAP)
			spare = static_cast<int16_t>(spare / 3);
		else
			spare = static_cast<int16_t>(spare / 2);

		// Otherwise centred WITHIN the box, never within the screen - the
		// difference is the gauges, and a page centred on the screen sits a
		// few pixels under them at every edge.
		return Point<int16_t>(
			static_cast<int16_t>(box.left() + (box.width() - size.x()) / 2),
			static_cast<int16_t>(box.top() + spare));
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
		// A new page starts at the top. Carrying the offset across would open
		// a short list already scrolled past its own end.
		list_scroll = 0;
		list_scroll_max = 0;

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
		// A FINISHED TRADE TURNS ITS OWN PAGE.
		//
		// "Trade complete." stayed on the second screen indefinitely: the
		// page had nothing left to do and no reason to change, and unlike the
		// main screen there is no scenery behind it to make that obvious. So
		// once the result has had its few seconds, go back to wherever the
		// player was before they opened the trade.
		if (current == TRADE)
		{
			// hosted(), not window(): window() BUILDS the page if it does not
			// exist, and a page that has never been opened cannot be a
			// finished trade.
			if (auto* table = dynamic_cast<UITrade*>(hosted(UIElement::Type::TRADE)))
			{
				if (table->is_finished())
				{
					go_back();

					return;
				}
			}
		}

		// WHO IS ON THIS MAP.
		//
		// Read from the map's own character list rather than the party or the
		// buddy list: trading is something you do with whoever is standing in
		// front of you, and the server will only accept an invitation to
		// somebody on the same map anyway.
		//
		// On a beat, not every frame - people walk about, but not that fast,
		// and the list is walked to build strings.
		// ...AND ON MAIL, so opening it notices whoever is standing here even
		// if you have never opened trade or party.
		if ((current == NEARBY || current == PARTY)
			&& --until_nearby <= 0)
		{
			until_nearby = 30;

			nearby.clear();
			nearby_names.clear();

			if (Stage::get().is_active())
			{
				if (MapObjects* chars = Stage::get().get_chars().get_chars())
				{
					for (auto& entry : *chars)
					{
						if (auto* who = static_cast<Char*>(entry.second.get()))
						{
							nearby.push_back(who->get_oid());

							// ANYBODY STANDING HERE IS SOMEBODY YOU PLAY
							// WITH, and therefore somebody you can write to
							// later without having to spell their name.
							PostBox::get().remember(who->get_name());
							nearby_names.push_back(who->get_name());
						}
					}
				}
			}
		}

		if (levelup_playing && levelup.update())
			levelup_playing = false;

		// Nothing announces a shop closing, so notice it here.
		if (guest && !guest->is_active())
			clear_guest();

		if (touching)
			hold_ticks++;

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
			drag_from = position;
			touching = true;
			touch_ticks = 0;
			hold_ticks = 0;
		}
		else if (touching)
		{
			touch_ticks++;
		}

		if (down)
		{

			// THE KEYBOARD TAKES THE PRESS BEFORE ANY PAGE DOES.
			if (keyboard_wanted() && keyboard_pressed(position, screen))
			{
				touching = false;

				return;
			}

			// WHILE A GUEST IS UP THE PANEL HAS NO CONTROLS OF ITS OWN.
			//
			// A shop covers the panel, but the address bar, the SPEAK and
			// TALK buttons and - worst - the whole MENU underneath it were
			// still hit-testing every press. Aiming at the shop's own exit
			// pressed whatever icon happened to be behind it, so closing a
			// shop landed you in the quest log. A window in front of the tree
			// is in front of it for touches too, not only for drawing.
			//
			// The SWIPES below still run: either one closes a guest, and that
			// is the way out of a shop that does not depend on hitting a
			// small X with a thumb.
			if (!has_guest())
			{

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

			if (current == KEYBINDS && keys_pressed(position, screen))
			{
				touching = false;

				return;
			}

			// A NAME ON THE PARTY PAGE: ask that person to join.
			if (current == PARTY && !nearby_names.empty())
			{
				Rectangle<int16_t> box = content_area(screen);

				const Party& party = Stage::get().get_player().get_party();

				int16_t rows = party.is_in_party()
					? static_cast<int16_t>(party.get_members().size()) : 0;

				if (rows > 6)
					rows = 6;

				constexpr int16_t ROW_H = 34;

				int16_t y = static_cast<int16_t>(
					box.top() + 26 + rows * ROW_H + 6 + 20);

				for (size_t i = 0; i < nearby_names.size() && i < 6; i++)
				{
					Rectangle<int16_t> row(
						Point<int16_t>(box.left(), y),
						Point<int16_t>(box.right(),
							static_cast<int16_t>(y + ROW_H - 6)));

					if (row.contains(position))
					{
						// BY NAME. The server looks the person up by name for
						// a party invitation, unlike a trade, which goes by
						// the character id.
						InviteToPartyPacket(nearby_names[i]).dispatch();

						touching = false;

						return;
					}

					y = static_cast<int16_t>(y + ROW_H);
				}
			}

			if (current == PARTY && party_action_box(screen).contains(position))
			{
				if (Stage::get().is_active())
				{
					if (Stage::get().get_player().get_party().is_in_party())
						LeavePartyPacket().dispatch();
					else
						CreatePartyPacket().dispatch();
				}

				touching = false;

				return;
			}

			if (current == EMOTIONS)
			{
				if (int16_t face = emotion_at(position, screen); face >= 0)
				{
					// The server takes the expression number straight; there
					// is no offset. Char::set_expression's byaction() knocks
					// 98 off, which belongs to the KEY BINDING ids (FACE1 is
					// 100), not to anything that goes over the wire.
					FaceExpressionPacket(FACES[face].id).dispatch();

					if (Stage::get().is_active())
						Stage::get().get_player().get_look().set_expression(
							static_cast<Expression::Id>(FACES[face].id));

					touching = false;

					return;
				}
			}

			// QUIT opens the game's own quit dialog, on the TOP screen.
			//
			// Deliberately not a panel page. Leaving the game is the one
			// action on here that should interrupt what you are looking at
			// rather than sit quietly on a page you might have opened by
			// accident - and the dialog already knows how to count down and
			// warn you if you are in combat.
			if (current == REPORT && report_box(screen).contains(position))
			{
				write_report();
				touching = false;

				return;
			}

			if (current == NEARBY)
			{
				for (size_t i = 0; i < nearby.size(); i++)
				{
					if (!nearby_row(static_cast<int16_t>(i), screen).contains(position))
						continue;

					// TWO PACKETS, ALWAYS IN THIS ORDER.
					//
					// The invitation on its own does nothing: the server
					// looks up the sender's trade, finds none, and gives up
					// without a word. CREATE has to go first, every time.
					TradeStartPacket().dispatch();
					TradeInvitePacket(nearby[i]).dispatch();

					show_page(TRADE);

					touching = false;

					return;
				}
			}

			// DUEY'S COUNTER. Two lists on one page: collect from the top,
			// send from the bottom.
			if (current == GIFT)
			{
				const std::vector<GiftBox::Parcel>& parcels =
					GiftBox::get().parcels();

				for (size_t i = 0; i < parcels.size() && i < 3; i++)
				{
					if (gift_parcel_row(i, screen).contains(position))
					{
						DueyClaimPacket(parcels[i].id).dispatch();

						// AND ASK AGAIN. The server does not send a fresh
						// counter on its own after a collection, so without
						// this the parcel stays on screen and can be tapped a
						// second time - which asks for something that is no
						// longer there.
						DueyOpenPacket().dispatch();

						touching = false;

						return;
					}
				}

				// A NAME IS ONLY A TARGET WHILE SOMETHING IS BEING CARRIED.
				//
				// Tapping one with an empty hand used to be the obvious way to
				// send nothing at all - the server answers that with a
				// disconnect, treating an amount of zero as a packet edit.
				if (carried.type != KeyType::Id::NONE)
				{
					const std::vector<std::string>& people =
						PostBox::get().known();

					for (size_t i = 0; i < people.size(); i++)
					{
						if (!gift_name_row(i, screen).contains(position))
							continue;

						const Inventory& bag =
							Stage::get().get_player().get_inventory();

						InventoryType::Id type =
							InventoryType::by_item_id(carried.action);

						int16_t slot = bag.find_item(type, carried.action);

						if (slot)
						{
							// THE WHOLE STACK. Duey takes a count, and asking
							// for one of a stack of thirty leaves the rest
							// behind with no way to say so on this page.
							int16_t amount = bag.get_item_count(type, slot);

							DueySendPacket(static_cast<int8_t>(type), slot,
								amount, 0, people[i]).dispatch();

							// Put it down. It is gone from the bag now, and a
							// carry that outlived the send would offer the
							// same item to the next name tapped.
							carried = Keyboard::Mapping();
						}
						else
						{
							GiftBox::get().set_result(
								"That is not in your bag any more.");
						}

						touching = false;

						return;
					}
				}
			}

			// START THE RUSH. Sent as the chat command a player could type -
			// v83 has no opcode for "begin a timed event", and inventing one
			// has meant a silent protocol failure every time on this project.
			// One route in, one thing to test.
			if (current == PVE && rush_box(screen).contains(position))
			{
				if (Stage::get().is_active())
					GeneralChatPacket("@rush", true).dispatch();

				touching = false;

				return;
			}

			if (current == ROOMCHAT)
			{
				const std::vector<std::string>& people =
					PostBox::get().known();

				for (size_t i = 0; i < people.size(); i++)
				{
					if (!message_row(static_cast<int16_t>(i), screen)
						.contains(position))
						continue;

					// Tapping the one already picked unpicks it, so there is
					// a way back out of a choice made by accident.
					message_to = (message_to == people[i])
						? std::string() : people[i];

					touching = false;

					return;
				}
			}

			if (current == ROOMCHAT && !message_to.empty()
				&& speak_box(screen).contains(position))
			{
				// The chat bar owns the recogniser and knows what to do with
				// a finished sentence, so this only asks it to start.
				// get_element returns an Optional, not a pointer.
				if (auto chat = UI::get().get_element<UIChatbar>())
					chat->start_dictation_post(message_to);

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

				// AND THE RELEASE BELONGS TO THIS PRESS, NOT TO THE NEW PAGE.
				//
				// Without this the UP that ends the same tap arrives with the
				// hotkey grid already showing, and the grid does the only
				// thing it can with a tap while you are carrying something:
				// it places it, wherever the stylus happened to be.
				swallow_up = true;

				return;
			}

			}   // !has_guest()
		}

		if (up && swallow_up)
		{
			swallow_up = false;
			touching = false;

			return;
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
					// The keys page is drawn BY the panel rather than hosted,
					// so there is no window to hand a scroll to. It keeps its
					// own offset - which is also why it can be clamped to its
					// own table length instead of guessing at a row count.
					if (current == KEYBINDS)
					{
						key_scroll = static_cast<int16_t>(key_scroll - step / 12);

						if (key_scroll < 0)
							key_scroll = 0;

						int16_t last = static_cast<int16_t>(
							bindable_count() > 4 ? bindable_count() - 4 : 0);

						if (key_scroll > last)
							key_scroll = last;
					}
					else if (UIElement* page = window())
					{
						page->send_scroll(step / 24.0);
					}

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
		// A VERTICAL DRAG ON ONE OF OUR OWN LISTS SCROLLS IT.
		//
		// The pages that host the game's own windows are not here: those have
		// their own sliders and send_scroll already reaches them.
		if (!up && touching
			&& (current == GIFT || current == NEARBY || current == ROOMCHAT))
		{
			int16_t dx = static_cast<int16_t>(position.x() - touch_start.x());
			int16_t dy = static_cast<int16_t>(position.y() - touch_start.y());

			int16_t adx = dx < 0 ? -dx : dx;
			int16_t ady = dy < 0 ? -dy : dy;

			if (ady > adx && ady > DRAG_SLOP)
				if (list_drag(position))
					return;
		}

		// NO SWIPE NAVIGATION. Left-to-right for back and right-to-left for
		// home are gone.
		//
		// They were the only way out of a page for a while, which is why they
		// existed - but the address bar along the top has been a row of
		// buttons for longer than that, and every crumb on it goes straight to
		// that level. So the gesture was a second, invisible copy of something
		// already on screen, and it fired by accident constantly: dragging a
		// list, panning a map, or a thumb that slid while pressing a button
		// all look like a swipe if they travel far enough.
		//
		// ⚠ The one-screen overlay used to be closed by swiping back at the
		// root. It is closed by pressing MENU again now, which is the same
		// button that opened it.

		// A menu is buttons and nothing else, so it never reaches a window.
		// Unless a guest is up, in which case the buttons are behind it and
		// the guest is what the finger is aiming at.
		if (is_menu(current) && !has_guest())
		{
			if (up && touching)
			{
				int16_t hit = menu_at(position, screen);

				size_t count = 0;
				const Page* items = menu_items(current, count);

				// An ACTION button does its thing where it stands. Only a
				// button that leads somewhere navigates.
				if (hit >= 0 && items && !menu_action(items[hit]))
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

			// MORE ROOM TO WANDER ON THE HOTKEY GRID.
			//
			// Its cells are 74 across and there is nothing to drag on the
			// page, so a thumb that slid ten pixels while pressing is
			// unambiguously still a press - and at DRAG_SLOP it was being
			// thrown away as a drag, which is what "not sensitive enough"
			// was. Nowhere else can afford this: the maps and the grids all
			// pan or scroll under the same finger.
			int16_t slop = (current == HOTKEYS)
				? static_cast<int16_t>(DRAG_SLOP * 3) : DRAG_SLOP;

			if (moved_x > slop || moved_y > slop)
			{
				highlighted = false;

				// Still a hover, so whatever it ended over stays lit.
				cursor_state = element->send_cursor(false, at);

				return;
			}

			// A HOLD ON A HOTKEY EMPTIES IT.
			//
			// Taken before the click below, because a short tap on the same
			// slot USES what is in it - so without this there was no gesture
			// left meaning "take it out". A slot could be overwritten but
			// never cleared, and a skill bound by mistake was permanent.
			//
			// Only on the hotkey page, and only after a release: clearing
			// while the finger is still down would fire the instant the timer
			// passed, with no way to change your mind by sliding off.
			if (current == HOTKEYS && hold_ticks >= HOLD_TO_CLEAR)
			{
				if (auto* keys = dynamic_cast<UIHotkeys*>(element))
					if (keys->clear_at(at))
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
			// A HELD POINTER THAT MOVES IS A DRAG, and it is offered as one
			// first.
			//
			// Everything here used to become a hover, pressed or not, which
			// is what makes a region on the world map light up as a finger
			// passes over it - and which meant the minimap, whose panning
			// only ever runs while the button is down, was told the button
			// was up on every single move. It could not be dragged at all,
			// by a finger or a stylus.
			//
			// A page that does not want the drag returns false and gets the
			// hover it always got.
			if (touching && element->send_drag(drag_from, position))
			{
				drag_from = position;

				return;
			}

			drag_from = position;

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

		// HOW HARD A BAR BREATHES, as an opacity. Full is steady; the emptier
		// it gets the faster and deeper it pulses, so running low is something
		// you notice out of the corner of your eye rather than something you
		// have to read.
		//
		// This was written and then never called - the colour mix below was
		// doing the whole job alone, which is why the warning was easy to miss
		// on a handheld held at arm's length. It is applied to the fill now,
		// so a low bar changes BRIGHTNESS as well as hue: two signals, and the
		// one that survives being seen out of the corner of the eye is the
		// brightness.
		//
		// Starts at 60%, not 50%. Half health on this server is one more hit
		// from a monster that hits twice.
		auto breathe = [this](float ratio) -> float
		{
			if (ratio >= 0.6f)
				return 1.0f;

			float urgency = (0.6f - ratio) / 0.6f;          // 0 at 60%, 1 at empty
			float speed = 0.06f + urgency * 0.26f;
			float wave = std::sin(pulse * speed) * 0.5f + 0.5f;

			return 1.0f - urgency * 0.62f * wave;
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

			// The same 60% the pulse starts at, so hue and brightness turn on
			// together rather than one arriving before the other.
			if (ratio < 0.6f)
			{
				float urgency = (0.6f - ratio) / 0.6f;   // 0 at 60%, 1 at empty
				float wave = std::sin(pulse * speed) * 0.5f + 0.5f;

				// Squared toward the end: the difference between a third full
				// and nearly empty should not be a gentle slope, it should be
				// the point where the bar starts shouting.
				heat = urgency * urgency * wave;
			}

			r = mr + (vr - mr) * heat;
			g = mg + (vg - mg) * heat;
			b = mb + (vb - mb) * heat;
		};

		float hr, hg, hb, mr2, mg2, mb2;

		// A LOUDER REST COLOUR AND A HARDER FLASH.
		//
		// The muted end was dusty enough that on a lit screen at arm's length
		// neither gauge read as red or blue so much as brownish and greyish -
		// so "how am I doing" needed a proper look. Both rest colours are
		// pushed toward their hue and the vivid ends are pure, which widens
		// the travel between them as well as raising the floor.
		//
		// The two speeds stay deliberately unmatched: in step, the pair would
		// beat together and neither would say WHICH of them is low.
		mix(hp_ratio, 0.130f, 0.78f, 0.20f, 0.20f, 1.00f, 0.10f, 0.08f, hr, hg, hb);
		mix(mp_ratio, 0.094f, 0.22f, 0.46f, 0.86f, 0.15f, 0.72f, 1.00f, mr2, mg2, mb2);

		// The pulse, as brightness. See breathe.
		float hp_breath = breathe(hp_ratio);
		float mp_breath = breathe(mp_ratio);

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
		// Nearly opaque, and dimmed by the pulse rather than by how empty it
		// is. 0.90 let the black trough through and greyed both colours.
		bar(RIM, static_cast<int16_t>(inner_top + inner_h - hp_h + RIM),
			VITAL_W - RIM * 2, static_cast<int16_t>(hp_h - RIM * 2),
			hr * hp_breath, hg * hp_breath, hb * hp_breath, 0.98f);
		bar(W - VITAL_W + RIM, static_cast<int16_t>(inner_top + inner_h - mp_h + RIM),
			VITAL_W - RIM * 2, static_cast<int16_t>(mp_h - RIM * 2),
			mr2 * mp_breath, mg2 * mp_breath, mb2 * mp_breath, 0.98f);

		// NO TOP STRIP.
		//
		// It was a black band across the whole width holding nothing but the
		// time, and it cut the panel's own frame off at the top - so the page
		// began below a bar that was not a gauge and did not need to be there.
		// The clock and the address bar sit straight on the frame now.
		if (!clock_icon.is_valid())
			clock_icon = nl::nx::map001["Custom"]["IconTime"];

		// THE POTION PIPS ARE GONE.
		//
		// A red bottle at the foot of the red gauge and a blue one at the foot
		// of the blue gauge label a colour with the same colour: they told you
		// nothing the bar had not already said, and they hung off the edges of
		// the panel to do it. The numbers directly above the EXP bar are what
		// answers "how much", and the gauges answer "how am I doing".
		//
		// IconHp and IconMp stay in Map001.nx - they are 400 bytes and the
		// hotkey bar may yet want them.

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

		// Clear of the GAUGES, which is all that is in those corners now that
		// the potion pips are gone. They used to be inset by the pip's width;
		// with the pips removed the numbers move back in to the bar's own
		// edge, which is as far out as they can go.
		hp_text.draw(Point<int16_t>(static_cast<int16_t>(VITAL_W + 4), num_y));
		mp_text.draw(Point<int16_t>(static_cast<int16_t>(W - VITAL_W - 4), num_y));

		// NO PERCENTAGE.
		//
		// The bar already says how far along the level is, and says it
		// continuously - a number to two decimal places is a precision nobody
		// acts on, sitting in the one place a page most wants to use. The bar
		// stays; only the figure goes.

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

	Point<int16_t> SecondScreenPanel::draw_art(const Texture& art,
		Rectangle<int16_t> box, int16_t pad, int16_t lift) const
	{
		// Returns the BOTTOM CENTRE of what was drawn, so a caller can put a
		// label under it without repeating the scaling arithmetic.
		Point<int16_t> middle(
			static_cast<int16_t>(box.left() + box.width() / 2),
			static_cast<int16_t>(box.top() + box.height() / 2));

		if (!art.is_valid())
			return middle;

		Point<int16_t> full = art.get_dimensions();

		int16_t room_w = static_cast<int16_t>(box.width() - pad * 2);
		int16_t room_h = static_cast<int16_t>(box.height() - pad * 2);

		if (full.x() <= 0 || full.y() <= 0 || room_w <= 0 || room_h <= 0)
			return middle;

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

		return Point<int16_t>(
			static_cast<int16_t>(at.x() + size.x() / 2),
			static_cast<int16_t>(at.y() + size.y()));
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

	// WHAT EACH BUTTON IS TRYING TO TELL YOU.
	//
	// Kept in one place so the rule for a page is written once, whether the
	// badge is drawn on the home screen or on the page that contains it.
	bool SecondScreenPanel::any_alert() const
	{
		size_t count = 0;

		// The HOME menu's own items, not every page there is: those six are
		// the ways in, and page_alert already carries a child's badge up to
		// its parent (Character shows one for skill points, Social for post).
		if (const Page* items = menu_items(HOME, count))
			for (size_t i = 0; i < count; i++)
				if (page_alert(items[i]))
					return true;

		return false;
	}

	bool SecondScreenPanel::page_alert(Page page) const
	{
		if (!Stage::get().is_active())
			return false;

		const CharStats& stats = Stage::get().get_player().get_stats();

		int16_t ap = stats.get_stat(Maplestat::Id::AP);
		int16_t sp = stats.get_stat(Maplestat::Id::SP);

		switch (page)
		{
		// The way in to both of the pages below, so it carries either.
		case CHARACTER:
			return ap > 0 || sp > 0;

		// NO POST BADGE. Mail arrives in the running chat now - it is
		// already in front of you, said out loud in the log, so a mark on the
		// way in would be pointing at something you have just read.

		// Ability points are spent on the stat sheet, skill points on skills.
		case ABILITY:
			return ap > 0;
		case SKILLS:
			return sp > 0;

		case DAILY:
		case PVE:
		{
			// Anything still unclaimed today. The same three tiers the daily
			// page itself draws - read from the server's own progress string,
			// so there is no second copy of the number.
			const std::string& said =
				Stage::get().get_player().get_quests().get_progress(DAILY_QUEST);

			int32_t kills = 0;

			if (!said.empty())
			{
				try
				{
					kills = std::stoi(said);
				}
				catch (...)
				{
					kills = 0;
				}
			}

			return kills < 300;
		}

		default:
			return false;
		}
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
			Point<int16_t> under = draw_art(page_art(items[i]), box, 10, 12);

			// THE BADGE, over the icon's top right corner.
			if (page_alert(items[i]))
			{
				if (!alert_tried)
				{
					alert_tried = true;
					alert_badge = nl::nx::map001["Custom"]["IconAlert"];
				}

				if (alert_badge.is_valid())
				{
					// Small: it is a mark ON the icon, not a second icon
					// beside it. Half the 28px artwork.
					constexpr int16_t BADGE = 14;

					alert_badge.draw(DrawArgument(
						Point<int16_t>(
							static_cast<int16_t>(box.right() - BADGE - 6),
							static_cast<int16_t>(box.top() + 6)),
						Point<int16_t>(BADGE, BADGE)));
				}
			}

			// THE NAME, DIRECTLY UNDER THE PICTURE.
			//
			// Not at the bottom of the button. A menu of two items gives each
			// one a box 236 pixels tall, so the icon sat in the middle and its
			// name was a hundred pixels below it - far enough that Map, Quest,
			// Voice and Messages all read as unlabelled. It is measured off
			// where the art actually landed now, so it follows the icon
			// whatever size the button is.
			//
			// The icons went label-less once they were in the breadcrumb, on
			// the theory that the trail teaches them. It teaches the ones you
			// have already pressed; it cannot teach a button you have never
			// opened, which is every button the first time.
			if (const char* name = page_name(items[i]))
			{
				menu_label.change_text(name);
				menu_label.draw(under + Point<int16_t>(0, 4));
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
		constexpr int16_t TOP = 4;

		Rectangle<int16_t> box = content_area(screen);

		int16_t left = box.left();
		int16_t width = box.width();

		for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
		{
			int16_t y = static_cast<int16_t>(box.top() + TOP + i * ROW_H);

			GraphicsGL::get().drawrectangle(
				left, y, width, ROW_H - 4, 1.0f, 1.0f, 1.0f, 0.14f);

			option_text.change_text(rows[i].name);
			option_text.draw(Point<int16_t>(static_cast<int16_t>(left + 8), y + 3));

			option_text.change_text(rows[i].value);
			option_text.draw(Point<int16_t>(
				static_cast<int16_t>(left + width - 90), y + 3));
		}
	}

	Rectangle<int16_t> SecondScreenPanel::emotion_box(size_t index,
		Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		constexpr int16_t COLS = 3;
		constexpr int16_t GAP = 8;

		int16_t left = box.left();
		int16_t TOP = static_cast<int16_t>(box.top() + 4);
		int16_t room = box.width();
		int16_t w = static_cast<int16_t>((room - GAP * (COLS - 1)) / COLS);

		// Three rows of faces plus the note underneath, sized to whatever the
		// box actually is rather than a fixed 56 that happened to fit once.
		int16_t h = static_cast<int16_t>((box.height() - 26 - GAP * 2) / 3);

		int16_t col = static_cast<int16_t>(index % COLS);
		int16_t row = static_cast<int16_t>(index / COLS);

		int16_t x = static_cast<int16_t>(left + col * (w + GAP));
		int16_t y = static_cast<int16_t>(TOP + row * (h + GAP));

		return Rectangle<int16_t>(
			Point<int16_t>(x, y),
			Point<int16_t>(static_cast<int16_t>(x + w),
				static_cast<int16_t>(y + h)));
	}

	int16_t SecondScreenPanel::emotion_at(Point<int16_t> at, Point<int16_t> screen) const
	{
		for (size_t i = 0; i < FACE_COUNT; i++)
			if (emotion_box(i, screen).contains(at))
				return static_cast<int16_t>(i);

		return -1;
	}

	void SecondScreenPanel::draw_emotions(Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		if (emotion_text.get_text().empty())
			emotion_text = Text(Text::Font::A12M, Text::Alignment::CENTER,
				Color::Name::WHITE);

		// YOUR OWN FACE, PULLING THE FACE.
		//
		// Not an icon standing in for it. The Face class already draws any
		// expression of the character's own face, so the button can BE the
		// thing it does - which is the only way "the emotions don't match"
		// stops being possible. It also means a different character with a
		// different face gets different buttons, for free.
		const ms::Face* mine = Stage::get().is_active()
			? Stage::get().get_player().get_look().get_face()
			: nullptr;

		for (size_t i = 0; i < FACE_COUNT; i++)
		{
			Rectangle<int16_t> box = emotion_box(i, screen);

			GraphicsGL::get().drawrectangle(
				box.left(), box.top(), box.width(), box.height(),
				1.0f, 1.0f, 1.0f, 0.14f);

			// THE KEY CONFIG'S OWN FACE, first choice.
			//
			// It is the picture the rest of the game uses for these seven, so
			// a face means the same thing wherever it appears - on a hotkey
			// slot, in the keyboard window, and here. The character's own
			// face is the fallback: truer to what you will actually pull, but
			// it is drawn at the size a character's head is, which on a
			// button reads as a smudge.
			Texture art = UIKeyConfig::action_icon(FACES[i].key);

			if (art.is_valid())
			{
				Rectangle<int16_t> inner(
					Point<int16_t>(box.left(), box.top() + 2),
					Point<int16_t>(box.right(), box.bottom() - 14));

				draw_art(art, inner, 4);
			}
			else if (mine)
			{
				// Faces are drawn from their CENTRE, unlike everything else
				// on this panel.
				Point<int16_t> at(
					static_cast<int16_t>(box.left() + box.width() / 2),
					static_cast<int16_t>(box.top() + box.height() / 2 - 4));

				mine->draw(static_cast<Expression::Id>(FACES[i].id), 0,
					DrawArgument(at));
			}

			emotion_text.change_text(FACES[i].name);
			emotion_text.draw(Point<int16_t>(
				static_cast<int16_t>(box.left() + box.width() / 2),
				static_cast<int16_t>(box.bottom() - 16)));
		}

		// WHAT IS MISSING, AND WHY.
		//
		// Sixteen more exist in the data. They are not hidden by us - the
		// server refuses any emote above 7 unless the matching item is in
		// your inventory, so a button for them would be a button that
		// silently does nothing.
		if (emotion_text.get_text() != "")
		{
			emotion_text.change_text("The other 16 need cash emote items.");
			emotion_text.draw(Point<int16_t>(
				static_cast<int16_t>(box.left() + box.width() / 2),
				static_cast<int16_t>(box.bottom() - 18)));
		}
	}

	void SecondScreenPanel::draw_daily(Point<int16_t> screen) const
	{
		if (daily_text.get_text().empty())
		{
			daily_text = Text(Text::Font::A12M, Text::Alignment::LEFT,
				Color::Name::WHITE);
			daily_count = Text(Text::Font::A13M, Text::Alignment::CENTER,
				Color::Name::WHITE);
		}

		// HOW MANY TODAY.
		//
		// The server keeps the count and publishes it as an info-quest string
		// - see DailyPve.PROGRESS_QUEST. Reading it here means no new packet
		// and no second copy of the number to keep in step: whatever the
		// server last said IS what this page shows.
		int32_t kills = 0;

		if (Stage::get().is_active())
		{
			const std::string& said =
				Stage::get().get_player().get_quests().get_progress(DAILY_QUEST);

			if (!said.empty())
			{
				try
				{
					kills = std::stoi(said);
				}
				catch (...)
				{
					kills = 0;
				}
			}
		}

		Rectangle<int16_t> box = content_area(screen);

		int16_t left = box.left();
		int16_t width = box.width();
		int16_t mid = static_cast<int16_t>(box.left() + box.width() / 2);
		int16_t top = box.top();

		GraphicsGL::get().drawrectangle(left, top, width, 46,
			1.0f, 1.0f, 1.0f, 0.12f);

		daily_count.change_text(std::to_string(kills) + " monsters today");
		daily_count.draw(Point<int16_t>(mid, top + 6));

		daily_text.change_text("Within 5 levels of you. Resets at midnight.");
		daily_text.draw(Point<int16_t>(left + 10, top + 26));

		// THE THREE TIERS, ticked or crossed.
		static const int32_t NEED[] = { 100, 200, 300 };
		static const int32_t PAYS[] = { 100, 200, 300 };

		constexpr int16_t ROW_H = 34;

		for (size_t i = 0; i < 3; i++)
		{
			int16_t y = static_cast<int16_t>(top + 56 + i * ROW_H);
			bool done = kills >= NEED[i];

			GraphicsGL::get().drawrectangle(left, y, width, ROW_H - 5,
				1.0f, 1.0f, 1.0f, 0.12f);

			// A GREEN TICK OR A RED CROSS, drawn from bars.
			//
			// No artwork: a tick and a cross have to read at a glance on a
			// screen an arm's length away, and the game's own small icons are
			// drawn for a mouse pointer resting on them.
			int16_t mx = static_cast<int16_t>(left + 18);
			int16_t my = static_cast<int16_t>(y + 8);

			auto bar = [&](int16_t x, int16_t yy, int16_t w, int16_t h)
			{
				GraphicsGL::get().drawrectangle(mx + x, my + yy, w, h,
					done ? 0.30f : 0.85f,
					done ? 0.80f : 0.22f,
					done ? 0.34f : 0.22f, 0.95f);
			};

			if (done)
			{
				bar(-6, 4, 5, 5);
				bar(-2, 8, 5, 5);
				bar(2, 4, 5, 5);
				bar(5, 0, 5, 5);
				bar(8, -4, 5, 5);
			}
			else
			{
				bar(-6, -4, 4, 4);
				bar(-2, 0, 4, 4);
				bar(2, 4, 4, 4);
				bar(2, -4, 4, 4);
				bar(-2, 0, 4, 4);
				bar(-6, 4, 4, 4);
			}

			daily_text.change_text(std::to_string(NEED[i]) + " monsters");
			daily_text.draw(Point<int16_t>(left + 44, y + 4));

			daily_text.change_text(std::to_string(PAYS[i]) + " NX");
			daily_text.draw(Point<int16_t>(
				static_cast<int16_t>(left + width - 60), y + 4));
		}

		// ---- THE THREE-MINUTE RUSH ------------------------------------------
		//
		// A different thing from the three tiers above: that one counts all
		// day and pays a fixed amount, this one is a sprint you choose to
		// start. The server publishes "secondsLeft:kills" through info quest
		// 7771, the same trick DailyPve uses for its count - so there is no
		// new packet and no second copy of the numbers.
		int32_t left_secs = -1;
		int32_t rush_kills = 0;

		if (Stage::get().is_active())
		{
			const std::string& said =
				Stage::get().get_player().get_quests().get_progress(RUSH_QUEST);

			size_t colon = said.find(':');

			if (colon != std::string::npos)
			{
				try
				{
					left_secs = std::stoi(said.substr(0, colon));
					rush_kills = std::stoi(said.substr(colon + 1));
				}
				catch (...)
				{
					left_secs = -1;
				}
			}
		}

		Rectangle<int16_t> rush = rush_box(screen);

		bool live = left_secs >= 0;

		GraphicsGL::get().drawrectangle(
			rush.left(), rush.top(), rush.width(), rush.height(),
			live ? 0.60f : 0.16f, live ? 0.30f : 0.44f,
			live ? 0.14f : 0.20f, 0.92f);

		// THE CLOCK IS THE WHOLE POINT while it is running, so it is what the
		// button says - not a label with a number beside it.
		daily_count.change_text(live
			? std::to_string(left_secs / 60) + ":"
				+ (left_secs % 60 < 10 ? "0" : "")
				+ std::to_string(left_secs % 60) + "  -  "
				+ std::to_string(rush_kills) + " down"
			: std::string("START THE 3 MINUTE RUSH"));

		daily_count.draw(Point<int16_t>(
			static_cast<int16_t>(rush.left() + rush.width() / 2),
			static_cast<int16_t>(rush.top() + 6)));

		daily_text.change_text(live
			? std::string("Kill as many as you can. Once a day.")
			: std::string("10 potions / 25 mesos / 50 +NX / 100 / 200 gachapon"));
		daily_text.draw(Point<int16_t>(
			rush.left() + 8, static_cast<int16_t>(rush.top() + 26)));
	}

	Rectangle<int16_t> SecondScreenPanel::rush_box(Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		constexpr int16_t H = 46;

		int16_t y = static_cast<int16_t>(box.bottom() - H);

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), y),
			Point<int16_t>(box.right(), static_cast<int16_t>(y + H)));
	}

	Rectangle<int16_t> SecondScreenPanel::report_box(Point<int16_t> screen) const
	{
		// FROM THE CONTENT BOX, like every other footer on the panel.
		//
		// These were each measured from the raw screen with their own guess
		// at the margins - VITAL_W + 12 here, + 8 there - so no two pages
		// agreed where the bottom was and several sat on the HP numbers.
		Rectangle<int16_t> box = content_area(screen);

		constexpr int16_t H = 38;

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), static_cast<int16_t>(box.bottom() - H)),
			Point<int16_t>(box.right(), box.bottom()));
	}

	void SecondScreenPanel::write_report() const
	{
		// WHAT A BUG REPORT NEEDS TO BE WORTH READING.
		//
		// "It broke" is not actionable and neither is a screenshot. What
		// costs a session is the things nobody thinks to mention: which build
		// they were on, which map, what they were wearing, whether the server
		// was theirs or somebody else's, and what the game had been saying to
		// itself just before it went wrong.
		//
		// All of it is already in memory. None of it is private - there are
		// no passwords here, no addresses beyond a LAN one, and it is written
		// to the player's own device, not sent anywhere. It is a note they
		// can choose to hand over.
		//
		// APPENDED, never overwritten: the second time something goes wrong
		// is usually the informative one, and a file that keeps only the last
		// report loses the pair.
		FILE* out = std::fopen("report.txt", "a");

		if (!out)
		{
			report_state = "Could not write the file.";

			return;
		}

		std::time_t now = std::time(nullptr);
		std::tm local {};

#ifdef _WIN32
		localtime_s(&local, &now);
#else
		localtime_r(&now, &local);
#endif

		char when[32];
		std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &local);

		std::fprintf(out, "================ %s ================\n", when);

		// THE BUILD. Without this every other line is guesswork - half the
		// reports in this project were about code that was not running.
		std::fprintf(out, "client   %s  (built %s %s)\n",
			Configuration::get().get_version().c_str(), __DATE__, __TIME__);

		std::fprintf(out, "screen   %d x %d   panel %d x %d\n",
			Setting<Width>::get().load(), Setting<Height>::get().load(),
			panel_screen.x(), panel_screen.y());

		std::fprintf(out, "server   %s\n", Setting<ServerIP>::get().load().c_str());
		std::fprintf(out, "page     %s\n",
			page_name(current) ? page_name(current) : "?");

		if (!Stage::get().is_active())
		{
			std::fprintf(out, "state    not in game\n\n");
			std::fclose(out);

			report_state = "Saved to report.txt";

			return;
		}

		const Player& me = Stage::get().get_player();
		const CharStats& stats = me.get_stats();

		std::fprintf(out, "\n-- character --\n");
		std::fprintf(out, "name     %s\n", stats.get_name().c_str());
		std::fprintf(out, "level    %d   job %d   exp %d\n",
			stats.get_stat(Maplestat::Id::LEVEL),
			stats.get_job().get_id(),
			static_cast<int32_t>(stats.get_exp()));
		std::fprintf(out, "hp       %d / %d      mp %d / %d\n",
			stats.get_stat(Maplestat::Id::HP),
			stats.get_total(Equipstat::Id::HP),
			stats.get_stat(Maplestat::Id::MP),
			stats.get_total(Equipstat::Id::MP));
		std::fprintf(out, "str/dex  %d / %d      int/luk %d / %d\n",
			stats.get_stat(Maplestat::Id::STR),
			stats.get_stat(Maplestat::Id::DEX),
			stats.get_stat(Maplestat::Id::INT),
			stats.get_stat(Maplestat::Id::LUK));
		std::fprintf(out, "ap / sp  %d / %d\n",
			stats.get_stat(Maplestat::Id::AP),
			stats.get_stat(Maplestat::Id::SP));
		std::fprintf(out, "damage   %d - %d\n",
			stats.get_mindamage(), stats.get_maxdamage());

		std::fprintf(out, "\n-- where --\n");
		std::fprintf(out, "map      %d\n", Stage::get().get_mapid());
		std::fprintf(out, "at       %d, %d\n",
			me.get_position().x(), me.get_position().y());

		// HOW BUSY THE MAP IS. A fault that only happens with a screenful of
		// monsters is a different fault from one that happens alone, and
		// nobody ever remembers to say which they were in.
		if (MapObjects* mobs = Stage::get().get_mobs().get_mobs())
			std::fprintf(out, "monsters %d\n", static_cast<int>(mobs->size()));

		if (MapObjects* npcs = Stage::get().get_npcs().get_npcs())
			std::fprintf(out, "npcs     %d\n", static_cast<int>(npcs->size()));

		if (MapObjects* chars = Stage::get().get_chars().get_chars())
			std::fprintf(out, "players  %d\n", static_cast<int>(chars->size()));

		// WHAT THEY ARE WEARING, because "my damage is wrong" is nearly
		// always answered by this list and never included with the question.
		std::fprintf(out, "\n-- worn --\n");

		const Inventory& bag = me.get_inventory();

		for (int16_t slot = 1; slot < 20; slot++)
		{
			if (int32_t id = bag.get_item_id(InventoryType::Id::EQUIPPED, slot))
				std::fprintf(out, "  %-3d %-8d %s\n", slot, id,
					ItemData::get(id).get_name().c_str());
		}

		for (int16_t slot = 101; slot < 120; slot++)
		{
			if (int32_t id = bag.get_item_id(InventoryType::Id::EQUIPPED, slot))
				std::fprintf(out, "  %-3d %-8d %s (cash)\n", slot, id,
					ItemData::get(id).get_name().c_str());
		}

		// AND THE LAST THING ANYONE SAID. Server notices, warnings and system
		// messages all arrive in the chat log, so the tail of it is usually
		// the closest thing to an error message the player ever saw.
		std::fprintf(out, "\n-- last said --\n");

		const std::deque<UIChatbar::Line>& said = UIChatbar::history();
		size_t from = said.size() > 12 ? said.size() - 12 : 0;

		for (size_t i = from; i < said.size(); i++)
			std::fprintf(out, "  %s\n", said[i].text.c_str());

		std::fprintf(out, "\n");
		std::fclose(out);

		report_state = "Saved. Report " + std::to_string(++report_count);
	}

	void SecondScreenPanel::draw_report(Point<int16_t> screen) const
	{
		if (report_text.get_text().empty())
			report_text = Text(Text::Font::A12M, Text::Alignment::CENTER,
				Color::Name::WHITE);

		Rectangle<int16_t> box = content_area(screen);

		int16_t left = box.left();
		int16_t width = box.width();
		int16_t mid = static_cast<int16_t>(box.left() + box.width() / 2);
		int16_t top = box.top();

		GraphicsGL::get().drawrectangle(left, top, width, 104,
			1.0f, 1.0f, 1.0f, 0.10f);

		report_text.change_text("Something wrong?");
		report_text.draw(Point<int16_t>(mid, top + 10));

		report_text.change_text("This saves where you were and");
		report_text.draw(Point<int16_t>(mid, top + 34));

		report_text.change_text("what you were doing to a file.");
		report_text.draw(Point<int16_t>(mid, top + 54));

		if (!report_state.empty())
		{
			report_text.change_text(report_state);
			report_text.draw(Point<int16_t>(mid, top + 80));
		}

		Rectangle<int16_t> act = report_box(screen);

		GraphicsGL::get().drawrectangle(
			act.left(), act.top(), act.width(), act.height(),
			0.52f, 0.16f, 0.16f, 0.90f);

		report_text.change_text("SAVE A REPORT");
		report_text.draw(Point<int16_t>(mid,
			static_cast<int16_t>(act.top() + act.height() / 2 - 8)));
	}

	namespace
	{
		// EVERY ACTION WORTH BINDING, AND WHAT TO CALL IT.
		//
		// KeyAction has no names - it is an enum and nothing in the client
		// ever needed to print one - so this is the table. It deliberately
		// leaves out the dozens of later-version entries (Legion, Profession,
		// Soul Weapon) that exist in the enum and do nothing in v83: a list
		// of sixty rows where forty are dead is not a list anybody can use.
		struct Bindable { const char* name; KeyAction::Id action; };

		const Bindable BINDABLE[] = {
			{ "Attack",         KeyAction::Id::ATTACK },
			{ "Jump",           KeyAction::Id::JUMP },
			{ "Pick up",        KeyAction::Id::PICKUP },
			{ "Sit",            KeyAction::Id::SIT },
			{ "Interact",       KeyAction::Id::INTERACT_HARVEST },

			{ "Inventory",      KeyAction::Id::ITEMS },
			{ "Equipment",      KeyAction::Id::EQUIPMENT },
			{ "Stats",          KeyAction::Id::STATS },
			{ "Skills",         KeyAction::Id::SKILLS },
			{ "Quest log",      KeyAction::Id::QUESTLOG },
			{ "World map",      KeyAction::Id::WORLDMAP },
			{ "Mini map",       KeyAction::Id::MINIMAP },
			{ "Key bindings",   KeyAction::Id::KEYBINDINGS },
			{ "Quick slots",    KeyAction::Id::QUICKSLOTS },
			{ "Menu",           KeyAction::Id::MENU },
			{ "Cash shop",      KeyAction::Id::CASHSHOP },

			{ "Friends",        KeyAction::Id::FRIENDS },
			{ "Party",          KeyAction::Id::PARTY },
			{ "Say",            KeyAction::Id::SAY },
			{ "Whisper",        KeyAction::Id::WHISPER },
			{ "Party chat",     KeyAction::Id::PARTYCHAT },
			{ "Toggle chat",    KeyAction::Id::TOGGLECHAT },

			{ "Face: blink",    KeyAction::Id::FACE1 },
			{ "Face: hit",      KeyAction::Id::FACE2 },
			{ "Face: smile",    KeyAction::Id::FACE3 },
			{ "Face: troubled", KeyAction::Id::FACE4 },
			{ "Face: cry",      KeyAction::Id::FACE5 },
			{ "Face: angry",    KeyAction::Id::FACE6 },
			{ "Face: puzzled",  KeyAction::Id::FACE7 },
		};

		constexpr size_t BINDABLE_COUNT = sizeof(BINDABLE) / sizeof(BINDABLE[0]);

		// What a key code looks like to a person. Only the ones a v83 keymap
		// actually uses; anything else is shown as its number, which is still
		// more use than a blank.
		std::string key_name(int32_t code)
		{
			if (code >= 'A' && code <= 'Z')
				return std::string(1, static_cast<char>(code));

			if (code >= '0' && code <= '9')
				return std::string(1, static_cast<char>(code));

			switch (code)
			{
			case 32:  return "Space";
			case 13:  return "Enter";
			case 16:  return "Shift";
			case 17:  return "Ctrl";
			case 18:  return "Alt";
			case 9:   return "Tab";
			case 27:  return "Esc";
			case 192: return "`";
			case 37:  return "Left";
			case 38:  return "Up";
			case 39:  return "Right";
			case 40:  return "Down";
			default:  break;
			}

			if (code >= 112 && code <= 123)
				return "F" + std::to_string(code - 111);

			return "#" + std::to_string(code);
		}
	}

	size_t SecondScreenPanel::bindable_count()
	{
		return BINDABLE_COUNT;
	}

	Rectangle<int16_t> SecondScreenPanel::key_row_box(size_t row,
		Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		constexpr int16_t ROW_H = 26;

		int16_t y = static_cast<int16_t>(box.top() + 4 + row * ROW_H);

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), y),
			Point<int16_t>(box.right(), static_cast<int16_t>(y + ROW_H - 3)));
	}

	Rectangle<int16_t> SecondScreenPanel::key_hotkey_box(Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		constexpr int16_t H = 30;

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), static_cast<int16_t>(box.bottom() - H)),
			Point<int16_t>(static_cast<int16_t>(box.left() + box.width() / 2 - 3),
				box.bottom()));
	}

	Rectangle<int16_t> SecondScreenPanel::key_bind_box(Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		constexpr int16_t H = 30;

		return Rectangle<int16_t>(
			Point<int16_t>(static_cast<int16_t>(box.left() + box.width() / 2 + 3),
				static_cast<int16_t>(box.bottom() - H)),
			Point<int16_t>(box.right(), box.bottom()));
	}

	void SecondScreenPanel::draw_keys(Point<int16_t> screen) const
	{
		if (key_text.get_text().empty())
		{
			key_text = Text(Text::Font::A12M, Text::Alignment::LEFT,
				Color::Name::WHITE);
			key_label = Text(Text::Font::A12M, Text::Alignment::CENTER,
				Color::Name::WHITE);
		}

		Rectangle<int16_t> box = content_area(screen);

		// How many rows fit above the two buttons.
		constexpr int16_t ROW_H = 26;

		int16_t rows = static_cast<int16_t>((box.height() - 36) / ROW_H);

		if (rows < 1)
			rows = 1;

		const std::map<int32_t, Keyboard::Mapping>& bound =
			UI::get().get_keyboard().get_maplekeys();

		for (int16_t i = 0; i < rows; i++)
		{
			size_t index = static_cast<size_t>(key_scroll + i);

			if (index >= BINDABLE_COUNT)
				break;

			Rectangle<int16_t> row = key_row_box(i, screen);
			bool picked = (static_cast<int16_t>(index) == key_selected);

			GraphicsGL::get().drawrectangle(
				row.left(), row.top(), row.width(), row.height(),
				picked ? 0.86f : 1.0f, picked ? 0.74f : 1.0f,
				picked ? 0.36f : 1.0f, picked ? 0.55f : 0.12f);

			key_text.change_text(BINDABLE[index].name);
			key_text.draw(Point<int16_t>(row.left() + 8, row.top() + 3));

			// WHAT IT IS BOUND TO NOW. Searched rather than looked up: the
			// keymap is key -> action, and this page reads action -> key.
			std::string on = "-";

			KeyType::Id want = UIKeyConfig::get_keytype(BINDABLE[index].action);

			for (const auto& entry : bound)
			{
				// Matched on the action's REAL type. Comparing against MENU
				// meant every ACTION and FACE row showed a dash however it
				// was bound - Jump and Attack read as unbound while sitting
				// on perfectly good keys.
				if (entry.second.action == BINDABLE[index].action
					&& entry.second.type == want)
				{
					on = key_name(entry.first);
					break;
				}
			}

			key_text.change_text(on);
			key_text.draw(Point<int16_t>(
				static_cast<int16_t>(row.right() - 70), row.top() + 3));
		}

		// THE TWO THINGS YOU CAN DO WITH THE ONE YOU PICKED.
		bool have = key_selected >= 0;

		auto button = [&](Rectangle<int16_t> at, const char* label, bool live,
			float r, float g, float b)
		{
			GraphicsGL::get().drawrectangle(
				at.left(), at.top(), at.width(), at.height(),
				live ? r : 0.16f, live ? g : 0.16f, live ? b : 0.16f,
				live ? 0.92f : 0.45f);

			key_label.change_text(label);
			key_label.draw(Point<int16_t>(
				static_cast<int16_t>(at.left() + at.width() / 2),
				static_cast<int16_t>(at.top() + 6)));
		};

		button(key_hotkey_box(screen), "TO HOTKEY", have, 0.18f, 0.44f, 0.20f);

		// The capture happens several layers below the UI, so this is where
		// the page finds out it succeeded.
		if (key_binding && PadBind::just_bound())
		{
			PadBind::clear_bound();
			key_binding = false;
		}

		button(key_bind_box(screen),
			key_binding ? "PRESS A BUTTON" : "CONTROLLER BIND",
			have || key_binding,
			key_binding ? 0.70f : 0.24f,
			key_binding ? 0.18f : 0.30f,
			key_binding ? 0.18f : 0.46f);
	}

	bool SecondScreenPanel::keys_pressed(Point<int16_t> at, Point<int16_t> screen)
	{
		constexpr int16_t ROW_H = 26;

		Rectangle<int16_t> box = content_area(screen);
		int16_t rows = static_cast<int16_t>((box.height() - 36) / ROW_H);

		for (int16_t i = 0; i < rows; i++)
		{
			size_t index = static_cast<size_t>(key_scroll + i);

			if (index >= BINDABLE_COUNT)
				break;

			if (key_row_box(i, screen).contains(at))
			{
				key_selected = static_cast<int16_t>(index);
				key_binding = false;

				return true;
			}
		}

		if (key_hotkey_box(screen).contains(at))
		{
			if (key_selected >= 0)
			{
				// THE ACTION'S OWN TYPE, not MENU for everything.
				//
				// A keymap entry is a (type, action) pair and the type is not
				// decoration: Jump and Attack are ACTION, the faces are FACE,
				// only the windows are MENU. Filing them all as MENU meant
				// Jump was bound as a menu that does not exist, so nothing
				// happened and nothing said why.
				//
				// UIKeyConfig::get_keytype already knows the answer for every
				// action in the game; there was never any need to guess.
				carried = Keyboard::Mapping(
					UIKeyConfig::get_keytype(BINDABLE[key_selected].action),
					BINDABLE[key_selected].action);

				go_to(HOTKEYS);

				// AND THE RELEASE STAYS WITH THIS PRESS.
				//
				// Without it the UP that ends this tap arrives on the hotkey
				// grid, which places whatever is carried into whichever slot
				// the finger happens to be over - so the binding landed in a
				// slot nobody chose, instantly, and the button looked broken
				// because it had finished before you could aim.
				swallow_up = true;
			}

			return true;
		}

		if (key_bind_box(screen).contains(at))
		{
			if (key_selected >= 0)
			{
				if (key_binding)
				{
					PadBind::cancel();
					key_binding = false;
				}
				else
				{
					// WHICH KEY THIS ACTION IS ON.
					//
					// A pad button holds a KEY CODE, not an action - the pad
					// is a keyboard in disguise. So binding "Jump" to A means
					// finding the key Jump is on and giving A that. An action
					// with no key cannot be bound to a button at all, which
					// is worth saying rather than arming a capture that can
					// never succeed.
					KeyType::Id want =
						UIKeyConfig::get_keytype(BINDABLE[key_selected].action);

					int32_t code = 0;

					for (const auto& entry : UI::get().get_keyboard().get_maplekeys())
					{
						if (entry.second.action == BINDABLE[key_selected].action
							&& entry.second.type == want)
						{
							code = entry.first;
							break;
						}
					}

					if (code != 0)
					{
						PadBind::arm(code);
						key_binding = true;
					}
				}
			}

			return true;
		}

		return false;
	}

	namespace
	{
		// FOUR ROWS, and the top one is digits rather than a number pad.
		//
		// A phone keyboard hides the digits behind a mode switch because it
		// is narrow. This is 344 wide and the row costs nothing, and the
		// things people type here - a character name, an account, a channel
		// number - are full of them.
		//
		// The last row is deliberately sparse: shift, symbols, space,
		// backspace and enter are the five that get pressed by mistake when
		// they are crowded, and enter is the one that must never be.
		const char* KB_ROWS[] = {
			"1234567890",
			"QWERTYUIOP",
			"ASDFGHJKL",
			"ZXCVBNM",
		};

		const char* KB_SYMS[] = {
			"1234567890",
			"!@#$%^&*()",
			"-_=+[]{};",
			":'\",.?/",
		};

		constexpr size_t KB_ROW_COUNT = 4;
	}

	bool SecondScreenPanel::keyboard_wanted() const
	{
		// AT THE LOGIN, ALWAYS. There is nothing else the lower screen can
		// usefully be while somebody is typing an account into the upper one,
		// and it goes the moment the game starts - which is the moment Login
		// is pressed and Stage becomes active.
		if (!Stage::get().is_active())
			return true;

		// AND ON THE CHAT PAGE, which exists to be typed into.
		//
		// NOT on Messages. The keyboard was drawn over that page for a while
		// and covered its lower half - the log it exists to show and the
		// SPEAK button both went under it. Messages reads; Chat types.
		// MAIL USED TO WANT IT TOO, while composing. That page is gone.
		//
		// The page exists to be read, and the keyboard covers its lower half.
		// It appears when WRITE is pressed and goes again when the message is
		// sent or abandoned.
		return current == SAY;
	}

	int16_t SecondScreenPanel::keyboard_top(Point<int16_t> screen) const
	{
		// AT THE LOGIN, LOW - the panel has nothing on it but a wordmark, so
		// the keys can have two thirds of the screen and be the size of a
		// thumb. This is the one place where somebody is typing something
		// they must get exactly right, into a box they cannot see, on the
		// other screen.
		if (!Stage::get().is_active())
			return static_cast<int16_t>(screen.y() * 3 / 10);

		// IN GAME, HALF - the chat above has to stay readable, and a sentence
		// in a game is a sentence, not a password.
		return static_cast<int16_t>(screen.y() / 2 + 6);
	}

	Rectangle<int16_t> SecondScreenPanel::key_cap_box(size_t row, size_t col,
		Point<int16_t> screen) const
	{
		// The keyboard owns the whole width, gauges included - it is not a
		// page, it is a keyboard, and a thumb-sized key matters more here
		// than a border does.
		constexpr int16_t GAP = 3;
		constexpr int16_t SIDE = 4;

		int16_t top = keyboard_top(screen);
		int16_t rows = static_cast<int16_t>(KB_ROW_COUNT + 1);

		int16_t h = static_cast<int16_t>((screen.y() - top - SIDE - GAP * rows) / rows);
		int16_t room = static_cast<int16_t>(screen.x() - SIDE * 2);

		// Each row is centred on its own width, so the shorter rows sit under
		// the middle of the longer ones the way a keyboard does.
		size_t len = (row < KB_ROW_COUNT)
			? std::strlen(kb_symbols ? KB_SYMS[row] : KB_ROWS[row])
			: 5;

		if (len == 0)
			len = 1;

		int16_t w = static_cast<int16_t>((room - GAP * 9) / 10);

		if (row == KB_ROW_COUNT)
		{
			// The bottom row: shift, symbols, space, TAB, back, enter. Space
			// takes whatever is left over because it is the one pressed most
			// and the one hardest to miss on purpose.
			//
			// TAB IS NOT DECORATION HERE. On a handheld the login form is on
			// the other screen, behind glass at the top of the device, so
			// there is nothing to tap to get from the account box to the
			// password box. Without this key a saved account name could never
			// be corrected, and a fresh one could never be followed by a
			// password - one field was all anybody could ever reach.
			int16_t x = SIDE;
			int16_t y = static_cast<int16_t>(top + row * (h + GAP));

			int16_t wide = static_cast<int16_t>(w * 2 + GAP);
			int16_t space = static_cast<int16_t>(room - (wide * 2 + w * 3 + GAP * 5));

			switch (col)
			{
			case 0: return Rectangle<int16_t>(Point<int16_t>(x, y),
				Point<int16_t>(x + wide, y + h));
			case 1: x = static_cast<int16_t>(x + wide + GAP);
				return Rectangle<int16_t>(Point<int16_t>(x, y),
					Point<int16_t>(x + w, y + h));
			case 2: x = static_cast<int16_t>(x + wide + GAP + w + GAP);
				return Rectangle<int16_t>(Point<int16_t>(x, y),
					Point<int16_t>(x + space, y + h));
			case 3: x = static_cast<int16_t>(x + wide + GAP + w + GAP + space + GAP);
				return Rectangle<int16_t>(Point<int16_t>(x, y),
					Point<int16_t>(x + w, y + h));
			case 4: x = static_cast<int16_t>(
					x + wide + GAP + w + GAP + space + GAP + w + GAP);
				return Rectangle<int16_t>(Point<int16_t>(x, y),
					Point<int16_t>(x + w, y + h));
			default: x = static_cast<int16_t>(screen.x() - SIDE - wide);
				return Rectangle<int16_t>(Point<int16_t>(x, y),
					Point<int16_t>(x + wide, y + h));
			}
		}

		int16_t used = static_cast<int16_t>(len * w + (len - 1) * GAP);
		int16_t left = static_cast<int16_t>((screen.x() - used) / 2);

		int16_t x = static_cast<int16_t>(left + col * (w + GAP));
		int16_t y = static_cast<int16_t>(top + row * (h + GAP));

		return Rectangle<int16_t>(
			Point<int16_t>(x, y),
			Point<int16_t>(static_cast<int16_t>(x + w),
				static_cast<int16_t>(y + h)));
	}

	void SecondScreenPanel::draw_keyboard(Point<int16_t> screen) const
	{
		if (kb_text.get_text().empty())
			// Dark letters. The ground is paper now, and white on paper is
			// the fault this was drawn to fix, only the other way up.
			kb_text = Text(Text::Font::A13M, Text::Alignment::CENTER,
				Color::Name::MINESHAFT);

		// PAPER UNDER THE WHOLE THING, EDGE TO EDGE.
		//
		// The panel's own parchment, stretched across the keyboard's share of
		// the screen. It is opaque, which is the point: in game the panel
		// shows the live map behind it, and the caps used to be white at a
		// tenth opacity with white letters on them - over a noon sky the keys
		// were very nearly invisible and the whole keyboard read as a smudge.
		//
		// Laying paper down first means the contrast no longer depends on
		// where you happen to be standing.
		int16_t plate = static_cast<int16_t>(keyboard_top(screen) - 6);
		int16_t deep = static_cast<int16_t>(screen.y() - plate);

		if (!kb_paper_tried)
		{
			kb_paper_tried = true;
			kb_paper = nl::nx::map001["Custom"]["BottomBg"];
		}

		if (kb_paper.is_valid())
			kb_paper.draw(DrawArgument(
				Point<int16_t>(0, plate), Point<int16_t>(screen.x(), deep)));
		else
			GraphicsGL::get().drawrectangle(
				0, plate, screen.x(), deep, 0.91f, 0.86f, 0.78f, 1.0f);

		auto cap = [&](Rectangle<int16_t> box, const std::string& label, bool lit)
		{
			// PRESSED INTO THE PAPER, not painted on top of it.
			//
			// A warm brown wash rather than a solid fill, so the parchment
			// grain still shows through and the caps read as part of the same
			// sheet. Lit keys take the green the panel uses everywhere else
			// for "this is on".
			GraphicsGL::get().drawrectangle(
				box.left(), box.top(), box.width(), box.height(),
				lit ? 0.36f : 0.58f,
				lit ? 0.52f : 0.46f,
				lit ? 0.34f : 0.32f,
				lit ? 0.55f : 0.30f);

			kb_text.change_text(label);
			kb_text.draw(Point<int16_t>(
				static_cast<int16_t>(box.left() + box.width() / 2),
				static_cast<int16_t>(box.top() + box.height() / 2 - 9)));
		};

		for (size_t row = 0; row < KB_ROW_COUNT; row++)
		{
			const char* keys = kb_symbols ? KB_SYMS[row] : KB_ROWS[row];

			for (size_t col = 0; keys[col]; col++)
			{
				char c = keys[col];

				// Shift only changes LETTERS. A shifted digit on this layout
				// would be a symbol nobody expects, and the symbol row is one
				// press away.
				if (!kb_symbols && !kb_shift && c >= 'A' && c <= 'Z')
					c = static_cast<char>(c - 'A' + 'a');

				cap(key_cap_box(row, col, screen), std::string(1, c), false);
			}
		}

		cap(key_cap_box(KB_ROW_COUNT, 0, screen), "SHIFT", kb_shift);
		// "!?" not "!#": a # begins a colour code in this renderer and
		// was being eaten, so the key read as a lone exclamation mark.
		cap(key_cap_box(KB_ROW_COUNT, 1, screen), kb_symbols ? "abc" : "!?", kb_symbols);
		cap(key_cap_box(KB_ROW_COUNT, 2, screen), "space", false);
		cap(key_cap_box(KB_ROW_COUNT, 3, screen), "next", false);
		cap(key_cap_box(KB_ROW_COUNT, 4, screen), "back", false);
		cap(key_cap_box(KB_ROW_COUNT, 5, screen), "ENTER", false);
	}

	bool SecondScreenPanel::keyboard_pressed(Point<int16_t> at, Point<int16_t> screen)
	{
		// WHO IS BEING TYPED INTO.
		//
		// On the chat page: this panel, into a line of its own. It does NOT
		// drive the chat window's textfield on the other screen - a focused
		// textfield swallows every key the game would otherwise get, so
		// opening one to type a sentence would take the controls away from
		// the player for as long as they were talking.
		//
		// At the login there is no page and no line: the characters go the
		// way SDL's own text input goes, to whichever field has focus over
		// there.
		const bool to_panel = Stage::get().is_active()
			&& current == SAY;
		// A CHARACTER IS NOT A KEYCODE.
		//
		// send_key runs a code through the KEYMAP and comes out with an
		// action - Jump, Inventory, a skill. It is the wrong door for typing
		// and it is why every letter did nothing: 'a' was being asked "which
		// game action is this?" rather than being put in the field.
		//
		// Printable text has its own path, the one SDL_TEXTINPUT uses, and a
		// field receives it as KeyType::TEXT. Editing keys - backspace,
		// enter, space - really are keys and still go through send_key.
		auto type_char = [&](char c)
		{
			// THE POST BOX USED TO GET FIRST REFUSAL HERE, while a message
			// was being composed. There is no compose page now - the chat
			// line is the only thing on this panel that is typed into.
			if (to_panel)
			{
				// A line long enough to run off the box is a line nobody can
				// check before sending. The game's own field stops at about
				// this too.
				if (say_line.size() < 70)
					say_line.push_back(c);

				return;
			}

			char one[2] = { c, 0 };

			UI::get().send_text(one);
		};

		auto tap = [&](int32_t code)
		{

			if (to_panel)
			{
				switch (code)
				{
				case GLFW_KEY_BACKSPACE:
					if (!say_line.empty())
						say_line.pop_back();

					return;
				case GLFW_KEY_ENTER:
					if (auto chat = UI::get().get_element<UIChatbar>())
						chat->say(say_line);

					say_line.clear();

					return;
				case GLFW_KEY_TAB:
					// Nothing to move to - there is one line on this page.
					return;
				default:
					return;
				}
			}

			UI::get().send_key(code, true);
			UI::get().send_key(code, false);
		};

		for (size_t row = 0; row < KB_ROW_COUNT; row++)
		{
			const char* keys = kb_symbols ? KB_SYMS[row] : KB_ROWS[row];

			for (size_t col = 0; keys[col]; col++)
			{
				if (!key_cap_box(row, col, screen).contains(at))
					continue;

				char c = keys[col];

				if (!kb_symbols && !kb_shift && c >= 'A' && c <= 'Z')
					c = static_cast<char>(c - 'A' + 'a');

				type_char((kb_shift || kb_symbols) ? keys[col] : c);

				// One shifted character, then back to lower case - the same
				// as every phone, and the alternative is a stuck modifier
				// nobody can see.
				kb_shift = false;

				return true;
			}
		}

		if (key_cap_box(KB_ROW_COUNT, 0, screen).contains(at))
		{
			kb_shift = !kb_shift;

			return true;
		}

		if (key_cap_box(KB_ROW_COUNT, 1, screen).contains(at))
		{
			kb_symbols = !kb_symbols;

			return true;
		}

		if (key_cap_box(KB_ROW_COUNT, 2, screen).contains(at))
		{
			// A space is a character, not a command.
			type_char(' ');

			return true;
		}

		if (key_cap_box(KB_ROW_COUNT, 3, screen).contains(at))
		{
			// "next" rather than "tab" on the cap: this is a phone, and on a
			// phone the key that moves to the following box says next.
			tap(GLFW_KEY_TAB);

			return true;
		}

		if (key_cap_box(KB_ROW_COUNT, 4, screen).contains(at))
		{
			tap(GLFW_KEY_BACKSPACE);

			return true;
		}

		if (key_cap_box(KB_ROW_COUNT, 5, screen).contains(at))
		{
			tap(GLFW_KEY_ENTER);

			return true;
		}

		return false;
	}

	bool SecondScreenPanel::menu_action(Page which)
	{
		// THE CASH SHOP TAKES OVER THE WHOLE CLIENT.
		//
		// Not a page: the server pulls the character out of the map and sends
		// the shop, which replaces everything on both screens. So this asks
		// and stops - there is nothing for the panel to navigate to, and
		// go_to would leave the trail pointing at a page that never draws.
		if (which == CASHSHOP)
		{
			if (Stage::get().is_active())
				OutPacket(OutPacket::Opcode::ENTER_CASHSHOP).dispatch();

			return true;
		}

		if (which != EXITGAME)
			return false;

		if (!Stage::get().is_active())
			return true;

		// GUARDED, LIKE THE STATUS BAR'S OWN QUIT BUTTON.
		//
		// UIQuit is built from artwork this UI data does not have. Raised
		// unguarded it draws nothing but a dark screen, takes the focus and
		// traps the player - and there is no Escape key on a handheld.
		// UIStatusbar carries a comment saying precisely that.
		if (UIQuit::has_artwork())
		{
			UI::get().emplace<UIQuit>(Stage::get().get_player().get_stats());
		}
		else
		{
			UI::get().emplace<UIYesNo>(
				"Do you want to return to the character select screen?",
				[](bool yes)
				{
					if (yes)
						UIQuit::return_to_charselect();
				}
			);
		}

		return true;
	}

	Rectangle<int16_t> SecondScreenPanel::party_action_box(Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		constexpr int16_t H = 34;

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), static_cast<int16_t>(box.bottom() - H)),
			Point<int16_t>(box.right(), box.bottom()));
	}

	void SecondScreenPanel::draw_party(Point<int16_t> screen) const
	{
		if (party_text.get_text().empty())
		{
			party_text = Text(Text::Font::A12M, Text::Alignment::LEFT,
				Color::Name::WHITE);
			party_head = Text(Text::Font::A13M, Text::Alignment::CENTER,
				Color::Name::WHITE);
		}

		if (!Stage::get().is_active())
			return;

		const Party& party = Stage::get().get_player().get_party();
		const auto& members = party.get_members();

		Rectangle<int16_t> box = content_area(screen);

		int16_t left = box.left();
		int16_t width = box.width();
		int16_t mid = static_cast<int16_t>(box.left() + box.width() / 2);

		bool grouped = party.is_in_party();
		bool alone = grouped && members.size() < 2;

		// WHO IS WITH YOU.
		party_head.change_text(
			!grouped ? "You are on your own"
			: (alone ? "A party with nobody in it yet" : "Your party"));

		party_head.draw(Point<int16_t>(mid, box.top() + 2));

		constexpr int16_t ROW_H = 34;
		int16_t TOP = static_cast<int16_t>(box.top() + 26);

		if (grouped && !members.empty())
		{
			int32_t leader = party.get_leader();

			for (size_t i = 0; i < members.size() && i < 6; i++)
			{
				const PartyMember& m = members[i];

				int16_t y = static_cast<int16_t>(TOP + i * ROW_H);

				GraphicsGL::get().drawrectangle(left, y, width, ROW_H - 5,
					1.0f, 1.0f, 1.0f, 0.12f);

				// The leader marked, because who can invite and expel is the
				// one fact about a party that changes what you can do.
				if (m.cid == leader)
					GraphicsGL::get().drawrectangle(left, y, 4, ROW_H - 5,
						1.0f, 0.86f, 0.30f, 0.90f);

				party_text.change_text(m.name);
				party_text.draw(Point<int16_t>(left + 12, y + 3));

				party_text.change_text("Lv." + std::to_string(m.level));
				party_text.draw(Point<int16_t>(
					static_cast<int16_t>(left + width - 92), y + 3));

				// Offline or on another map is worth saying: it explains why
				// somebody is in the list and not beside you.
				party_text.change_text(!m.online ? "away"
					: (m.mapid == Stage::get().get_mapid() ? "here" : "elsewhere"));

				party_text.draw(Point<int16_t>(
					static_cast<int16_t>(left + width - 46), y + 3));
			}
		}
		else
		{
			GraphicsGL::get().drawrectangle(left, TOP, width, 56,
				1.0f, 1.0f, 1.0f, 0.10f);

			party_text.change_text("A party shares experience and lets you");
			party_text.draw(Point<int16_t>(left + 10, TOP + 10));

			party_text.change_text("see each other on the map.");
			party_text.draw(Point<int16_t>(left + 10, TOP + 30));
		}

		// WHO ELSE IS HERE, AND A TAP TO ASK THEM.
		//
		// The party page could create a party and leave one and offered no
		// way to put anybody IN it: the invite list lives in the main
		// screen's HUD, which this panel replaces. So the page could make an
		// empty party and nothing else, and the other player never appeared
		// anywhere.
		//
		// Same list the trade page uses, for the same reason - it is whoever
		// is standing here, which is who you would ask.
		{
			int16_t rows = grouped ? static_cast<int16_t>(members.size()) : 0;

			if (rows > 6)
				rows = 6;

			int16_t y = static_cast<int16_t>(TOP + rows * ROW_H + 6);

			party_text.change_text(nearby_names.empty()
				? "Nobody else on this map"
				: "Tap a name to invite them");

			party_text.draw(Point<int16_t>(left + 6, y));

			y = static_cast<int16_t>(y + 20);

			for (size_t i = 0; i < nearby_names.size() && i < 6; i++)
			{
				Rectangle<int16_t> row(
					Point<int16_t>(left, y),
					Point<int16_t>(static_cast<int16_t>(left + width),
						static_cast<int16_t>(y + ROW_H - 6)));

				GraphicsGL::get().drawrectangle(row.left(), row.top(),
					row.width(), row.height(), 0.18f, 0.34f, 0.22f, 0.92f);

				party_text.change_text(nearby_names[i]);
				party_text.draw(Point<int16_t>(row.left() + 8, row.top() + 5));

				y = static_cast<int16_t>(y + ROW_H);
			}
		}

		// ONE BUTTON, AND IT SAYS WHICH ONE IT IS.
		//
		// Create when you have no party, leave when you have. Two buttons
		// where only one can ever apply is two things to read.
		Rectangle<int16_t> act = party_action_box(screen);

		GraphicsGL::get().drawrectangle(
			act.left(), act.top(), act.width(), act.height(),
			grouped ? 0.52f : 0.18f,
			grouped ? 0.18f : 0.44f,
			grouped ? 0.18f : 0.20f, 0.92f);

		party_head.change_text(grouped ? "LEAVE THE PARTY" : "CREATE A PARTY");
		party_head.draw(Point<int16_t>(
			mid, static_cast<int16_t>(act.top() + 7)));
	}

	Rectangle<int16_t> SecondScreenPanel::nearby_row(int16_t index,
		Point<int16_t> screen) const
	{
		constexpr int16_t ROW_H = 30;

		Rectangle<int16_t> box = content_area(screen);

		Point<int16_t> at(box.left(),
			static_cast<int16_t>(box.top() + 26 + index * ROW_H));

		return Rectangle<int16_t>(at, at + Point<int16_t>(box.width(), ROW_H - 4));
	}

	void SecondScreenPanel::draw_nearby(Point<int16_t> screen) const
	{
		if (nearby_text.get_text().empty())
			nearby_text = Text(Text::Font::A12M, Text::Alignment::LEFT,
				Color::Name::WHITE);

		Rectangle<int16_t> box = content_area(screen);

		nearby_text.change_text("Tap someone to ask them to trade.");
		nearby_text.draw(Point<int16_t>(box.left(), box.top() + 2));

		if (nearby.empty())
		{
			nearby_text.change_text("Nobody else is on this map.");
			nearby_text.draw(Point<int16_t>(box.left(), box.top() + 34));

			return;
		}

		for (size_t i = 0; i < nearby.size(); i++)
		{
			Rectangle<int16_t> row = nearby_row(static_cast<int16_t>(i), screen);

			if (row.bottom() > box.bottom())
				break;

			GraphicsGL::get().drawrectangle(
				row.left(), row.top(), row.width(), row.height(),
				0.11f, 0.13f, 0.16f, 0.90f);

			nearby_text.change_text(nearby_names[i]);
			nearby_text.draw(Point<int16_t>(
				static_cast<int16_t>(row.left() + 10),
				static_cast<int16_t>(row.top() + 5)));
		}
	}



	bool SecondScreenPanel::list_drag(Point<int16_t> position)
	{
		if (list_scroll_max <= 0)
			return false;

		int16_t moved = static_cast<int16_t>(position.y() - drag_from.y());

		if (moved == 0)
			return false;

		// DRAG THE CONTENT, NOT THE WINDOW. A finger moving DOWN pulls the
		// list down, which shows what is above - the same way every list on
		// a touchscreen has worked since 2007. Subtracting instead of adding
		// here is the single most noticeable thing you can get wrong.
		list_scroll = static_cast<int16_t>(list_scroll - moved);

		if (list_scroll < 0)
			list_scroll = 0;

		if (list_scroll > list_scroll_max)
			list_scroll = list_scroll_max;

		drag_from = position;

		return true;
	}

	void SecondScreenPanel::draw_scrollbar(Rectangle<int16_t> box,
		int16_t content) const
	{
		int16_t room = box.height();

		// NOTHING TO SAY WHEN EVERYTHING FITS. A track drawn down a page that
		// does not scroll is furniture pretending to be a control.
		if (content <= room)
		{
			list_scroll_max = 0;

			return;
		}

		list_scroll_max = static_cast<int16_t>(content - room);

		constexpr int16_t W = 3;

		int16_t x = static_cast<int16_t>(box.right() - W);

		GraphicsGL::get().drawrectangle(
			x, box.top(), W, room, 1.0f, 1.0f, 1.0f, 0.10f);

		// The thumb is as tall a share of the track as the page is of the
		// content, which is what makes it say HOW MUCH more there is rather
		// than only which way.
		int16_t thumb = static_cast<int16_t>(room * room / content);

		if (thumb < 18)
			thumb = 18;

		int16_t travel = static_cast<int16_t>(room - thumb);
		int16_t at = list_scroll_max > 0
			? static_cast<int16_t>(travel * list_scroll / list_scroll_max)
			: 0;

		GraphicsGL::get().drawrectangle(
			x, static_cast<int16_t>(box.top() + at), W, thumb,
			0.85f, 0.80f, 0.45f, 0.85f);
	}

	// ------------------------------------------------------------ duey's counter

	int16_t SecondScreenPanel::gift_split(Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		size_t waiting = GiftBox::get().parcels().size();

		// Never more than two parcels' worth of room, however many are there.
		// The people below have to stay reachable - a counter full of gifts
		// must not push the way to SEND one off the bottom - and the rest are
		// still collectable one at a time as the top ones go.
		if (waiting > 2)
			waiting = 2;

		return static_cast<int16_t>(
			box.top() + HEAD_H + waiting * GIFT_ROW_H + 8);
	}

	Rectangle<int16_t> SecondScreenPanel::gift_parcel_row(size_t index,
		Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		int16_t y = static_cast<int16_t>(box.top() + HEAD_H + index * GIFT_ROW_H);

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), y),
			Point<int16_t>(box.right(), static_cast<int16_t>(y + GIFT_ROW_H - 4)));
	}

	Rectangle<int16_t> SecondScreenPanel::gift_name_row(size_t index,
		Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		// HEAD_H below the rule for the "Sending:" line, and shifted by
		// however far the list is scrolled.
		int16_t y = static_cast<int16_t>(
			gift_split(screen) + HEAD_H + index * GIFT_ROW_H - list_scroll);

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), y),
			Point<int16_t>(static_cast<int16_t>(box.right() - 6),
				static_cast<int16_t>(y + GIFT_ROW_H - 4)));
	}

	void SecondScreenPanel::draw_gift(Point<int16_t> screen) const
	{
		if (gift_text.get_text().empty())
		{
			gift_text = Text(Text::Font::A12M, Text::Alignment::LEFT,
				Color::Name::WHITE);
			gift_small = Text(Text::Font::A11M, Text::Alignment::LEFT,
				Color::Name::WHITE);
		}

		Rectangle<int16_t> box = content_area(screen);

		const std::vector<GiftBox::Parcel>& parcels = GiftBox::get().parcels();

		gift_text.change_text(parcels.empty()
			? "Nothing waiting for you."
			: "Waiting for you - tap to collect.");
		gift_text.draw(Point<int16_t>(box.left(), box.top()));

		for (size_t i = 0; i < parcels.size() && i < 2; i++)
		{
			const GiftBox::Parcel& parcel = parcels[i];

			Rectangle<int16_t> row = gift_parcel_row(i, screen);

			GraphicsGL::get().drawrectangle(
				row.left(), row.top(), row.width(), row.height(),
				0.13f, 0.18f, 0.13f, 0.90f);

			gift_text.change_text("from " + parcel.from);
			gift_text.draw(Point<int16_t>(
				static_cast<int16_t>(row.left() + 8),
				static_cast<int16_t>(row.top() + 2)));

			// WHAT IS IN IT, BY NAME. An item id is not an answer to "what did
			// he send me" - the same lookup the bag uses, so a gift reads the
			// way the item does everywhere else.
			std::string what;

			if (parcel.item)
			{
				what = ItemData::get(parcel.item).get_name();

				if (parcel.count > 1)
					what += " x" + std::to_string(parcel.count);
			}

			if (parcel.mesos > 0)
			{
				if (!what.empty())
					what += " and ";

				what += std::to_string(parcel.mesos) + " mesos";
			}

			gift_small.change_text(what.empty() ? std::string("(empty)") : what);
			gift_small.draw(Point<int16_t>(
				static_cast<int16_t>(row.left() + 8),
				static_cast<int16_t>(row.top() + 19)));
		}

		// ---- and the sending half -------------------------------------------

		int16_t split = gift_split(screen);

		GraphicsGL::get().drawrectangle(
			box.left(), split, box.width(), 1, 1.0f, 1.0f, 1.0f, 0.20f);

		// WHAT IS BEING SENT, WHICH IS WHATEVER YOU PICKED UP IN THE BAG.
		//
		// The same carry the hotkey page uses - choose a potion on the item
		// page, come here, choose a name. No item picker of its own: the bag
		// IS the item picker, and a second one would be a second thing to
		// keep in step with the inventory.
		bool holding = carried.type != KeyType::Id::NONE;

		gift_text.change_text(holding
			? "Sending: " + ItemData::get(carried.action).get_name()
			: std::string("Pick something in your bag first."));
		gift_text.draw(Point<int16_t>(
			box.left(), static_cast<int16_t>(split + 5)));

		const std::vector<std::string>& people = PostBox::get().known();

		// THE LIST STOPS SHORT OF THE BOTTOM LINE, which is where Duey's
		// answer is printed. Without this the last name and the reply were
		// drawn on the same pixels, which is exactly the collision that was
		// reported.
		Rectangle<int16_t> list(
			Point<int16_t>(box.left(), static_cast<int16_t>(split + HEAD_H)),
			Point<int16_t>(box.right(), static_cast<int16_t>(box.bottom() - FOOT_H)));

		if (people.empty())
		{
			gift_small.change_text("Nobody to send to yet.");
			gift_small.draw(Point<int16_t>(list.left(), list.top() + 4));
		}

		for (size_t i = 0; i < people.size(); i++)
		{
			Rectangle<int16_t> row = gift_name_row(i, screen);

			// CLIPPED TO THE LIST, top and bottom. A scrolled row does not
			// stop existing, it moves - and one that has moved above the
			// heading would otherwise be drawn straight over it.
			if (row.bottom() <= list.top() || row.top() >= list.bottom())
				continue;

			GraphicsGL::get().drawrectangle(
				row.left(), row.top(), row.width(), row.height(),
				holding ? 0.16f : 0.11f, holding ? 0.20f : 0.12f,
				holding ? 0.26f : 0.13f, 0.90f);

			gift_text.change_text(people[i]);
			gift_text.draw(Point<int16_t>(
				static_cast<int16_t>(row.left() + 10),
				static_cast<int16_t>(row.top() + 4)));
		}

		draw_scrollbar(list,
			static_cast<int16_t>(people.size() * GIFT_ROW_H));

		// WHAT DUEY LAST SAID, on its own line along the very bottom - an
		// answer to something already done rather than part of doing it.
		const std::string& result = GiftBox::get().result();

		if (!result.empty())
		{
			gift_small.change_text(result);
			gift_small.draw(Point<int16_t>(
				box.left(), static_cast<int16_t>(box.bottom() - FOOT_H + 2)));
		}
	}

	Rectangle<int16_t> SecondScreenPanel::say_box(Point<int16_t> screen) const
	{
		// The line being typed, directly above the keys. Not at the top of
		// the page: what you are writing wants to be next to the thing you
		// are writing it with, so a thumb and an eye are in the same place.
		constexpr int16_t H = 26;

		int16_t top = static_cast<int16_t>(keyboard_top(screen) - H - 4);

		Rectangle<int16_t> box = content_area(screen);

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), top),
			Point<int16_t>(box.right(), static_cast<int16_t>(top + H)));
	}


	// ------------------------------------------------------------ the post box

	Rectangle<int16_t> SecondScreenPanel::message_row(int16_t index,
		Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		// The same row height and the same scroll offset the gift list uses,
		// so the two name lists behave identically - they are the same list
		// of people asked two different questions.
		int16_t y = static_cast<int16_t>(
			box.top() + HEAD_H + index * GIFT_ROW_H - list_scroll);

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), y),
			Point<int16_t>(static_cast<int16_t>(box.right() - 6),
				static_cast<int16_t>(y + GIFT_ROW_H - 4)));
	}

	// WHERE THE PAGE ACTUALLY ENDS.
	//
	// While writing, the keyboard owns the bottom of the panel. Measuring
	// these from the content box put every field UNDERNEATH it: the text went
	// into a box nobody could see and SEND could not be reached at all, which
	// read exactly like a keyboard that did nothing.
	void SecondScreenPanel::draw_chat(Point<int16_t> screen) const
	{
		if (bubble_text.get_text().empty())
		{
			bubble_text = Text(Text::Font::A11M, Text::Alignment::LEFT,
				Color::Name::WHITE);
			say_text = Text(Text::Font::A12M, Text::Alignment::LEFT,
				Color::Name::WHITE);
		}

		Rectangle<int16_t> box = content_area(screen);
		Rectangle<int16_t> line = say_box(screen);

		int16_t left = box.left();
		int16_t top = box.top();
		int16_t room = static_cast<int16_t>(line.top() - 4 - top);

		// BALLOONS, NEWEST AT THE BOTTOM.
		//
		// Drawn from the last line upward, so the thing just said is always
		// in the same place whether there are two lines or eighty. The colour
		// is the game's own: yellow for the system, blue and red for the
		// channels, white for people talking.
		const std::deque<UIChatbar::Line>& lines = UIChatbar::history();

		constexpr int16_t ROW_H = 19;
		constexpr int16_t PAD_X = 6;

		int16_t rows = static_cast<int16_t>(room / ROW_H);
		int16_t y = static_cast<int16_t>(top + room - ROW_H);

		for (int16_t i = 0; i < rows && i < static_cast<int16_t>(lines.size()); i++)
		{
			const UIChatbar::Line& said = lines[lines.size() - 1 - i];

			bubble_text.change_text(said.text);

			// The balloon is measured to the words rather than run to the
			// edge of the page. A full-width plate behind every line is a
			// table, and a table of one-word remarks looks like nothing was
			// said.
			int16_t width = static_cast<int16_t>(
				bubble_text.width() + PAD_X * 2);

			int16_t most = static_cast<int16_t>(box.width() - 8);

			if (width > most)
				width = most;

			// THE SYSTEM TALKS FROM THE OTHER SIDE.
			//
			// Notices, drop messages and quest text are not somebody in the
			// room, and putting them in the same place as people made the
			// page unreadable at a glance. They sit right, dimmer; people sit
			// left, brighter. Which side a balloon is on is the fastest thing
			// on the page to read.
			bool mine = said.type == UIChatbar::LineType::WHITE;

			int16_t at = mine
				? left
				: static_cast<int16_t>(box.right() - width);

			GraphicsGL::get().drawrectangle(
				at, y, width, static_cast<int16_t>(ROW_H - 3),
				mine ? 0.14f : 0.10f,
				mine ? 0.17f : 0.11f,
				mine ? 0.21f : 0.13f,
				mine ? 0.88f : 0.72f);

			bubble_text.draw(Point<int16_t>(
				static_cast<int16_t>(at + PAD_X),
				static_cast<int16_t>(y - 2)));

			y = static_cast<int16_t>(y - ROW_H);
		}

		if (lines.empty())
		{
			bubble_text.change_text("Nothing said yet. Type below.");
			bubble_text.draw(Point<int16_t>(
				static_cast<int16_t>(left + PAD_X), top + 8));
		}

		// THE LINE BEING TYPED, with a caret so it is plainly live even when
		// it is empty.
		GraphicsGL::get().drawrectangle(
			line.left(), line.top(), line.width(), line.height(),
			0.16f, 0.30f, 0.20f, 0.92f);

		// JUST THE CARET WHEN IT IS EMPTY.
		//
		// "Type, then ENTER" is what a green box with a cursor in it already
		// says, and on a panel this size a line of instructions is a line of
		// somebody else's message that cannot be shown.
		say_text.change_text(say_line + "_");

		say_text.draw(Point<int16_t>(
			static_cast<int16_t>(line.left() + PAD_X),
			static_cast<int16_t>(line.top() + 4)));
	}

	Rectangle<int16_t> SecondScreenPanel::speak_box(Point<int16_t> screen) const
	{
		Rectangle<int16_t> box = content_area(screen);

		constexpr int16_t H = 32;

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), static_cast<int16_t>(box.bottom() - H)),
			Point<int16_t>(box.right(), box.bottom()));
	}

	void SecondScreenPanel::draw_messages(Point<int16_t> screen) const
	{
		// A MESSAGE TO SOMEBODY WHO IS NOT HERE.
		//
		// This page used to be the room's chat log with a microphone under it,
		// which is now what R3 does from anywhere - a stick click, a bubble
		// over your head, no page to open. What it did NOT cover is the one
		// thing the chat cannot: reaching somebody who is not connected.
		//
		// So it is a list of people and a SPEAK button. Pick a name, say the
		// words. If they are online it reaches them in moments; if they are
		// not it waits in the outbox and lands in their running chat when they
		// next log in, from another state if a relay is set.
		if (message_text.get_text().empty())
		{
			message_text = Text(Text::Font::A12M, Text::Alignment::LEFT,
				Color::Name::WHITE);
			speak_text = Text(Text::Font::A13M, Text::Alignment::CENTER,
				Color::Name::WHITE);
		}

		Rectangle<int16_t> box = content_area(screen);
		Rectangle<int16_t> say = speak_box(screen);

		size_t queued = PostBox::get().outbox().size();

		message_text.change_text(queued > 0
			? "Waiting to go out: " + std::to_string(queued)
			: std::string("Pick a name, then speak."));
		message_text.draw(Point<int16_t>(box.left(), box.top()));

		const std::vector<std::string>& people = PostBox::get().known();

		// The list stops above the SPEAK button, which is the whole width of
		// the page - a name drawn behind it could be tapped and never seen.
		Rectangle<int16_t> list(
			Point<int16_t>(box.left(), static_cast<int16_t>(box.top() + HEAD_H)),
			Point<int16_t>(box.right(), static_cast<int16_t>(say.top() - 6)));

		if (people.empty())
		{
			message_text.change_text("Nobody to write to yet.");
			message_text.draw(Point<int16_t>(list.left(), list.top() + 4));
		}

		for (size_t i = 0; i < people.size(); i++)
		{
			Rectangle<int16_t> row = message_row(
				static_cast<int16_t>(i), screen);

			if (row.bottom() <= list.top() || row.top() >= list.bottom())
				continue;

			bool picked = (people[i] == message_to);

			GraphicsGL::get().drawrectangle(
				row.left(), row.top(), row.width(), row.height(),
				picked ? 0.20f : 0.11f, picked ? 0.30f : 0.13f,
				picked ? 0.22f : 0.16f, 0.90f);

			message_text.change_text(people[i]);
			message_text.draw(Point<int16_t>(
				static_cast<int16_t>(row.left() + 10),
				static_cast<int16_t>(row.top() + 4)));
		}

		draw_scrollbar(list,
			static_cast<int16_t>(people.size() * GIFT_ROW_H));

		// SPEAK, not type. None of these devices has a keyboard, and the
		// recogniser is already here for the chat bar's microphone button.
		bool can = Speech::get().available();
		bool listening = Speech::get().is_listening();
		bool ready = can && !message_to.empty();

		GraphicsGL::get().drawrectangle(
			say.left(), say.top(), say.width(), say.height(),
			listening ? 0.60f : 0.16f,
			listening ? 0.18f : 0.34f,
			listening ? 0.18f : 0.42f,
			ready ? 0.92f : 0.40f);

		// THE BUTTON SAYS WHO IT IS FOR. Without the name on it there was
		// nothing on screen tying the list above to the button below, and
		// pressing it with nobody picked looked like a button that did
		// nothing.
		speak_text.change_text(
			listening ? "LISTENING..."
			: !can ? std::string("NO MICROPHONE")
			: message_to.empty() ? std::string("PICK A NAME FIRST")
			: "SPEAK TO " + message_to);

		speak_text.draw(Point<int16_t>(
			static_cast<int16_t>(say.left() + say.width() / 2),
			static_cast<int16_t>(say.top() + 4)));
	}

	Rectangle<int16_t> SecondScreenPanel::talk_box(Point<int16_t> screen) const
	{
		// Most of the page, low down, where a thumb already is - but inside
		// the content box, so it does not reach over the EXP gauge.
		Rectangle<int16_t> box = content_area(screen);

		return Rectangle<int16_t>(
			Point<int16_t>(box.left(), static_cast<int16_t>(box.top() + 30)),
			Point<int16_t>(box.right(), box.bottom()));
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
		case CASHSHOP:  art = custom["IconCashShop"]; break;
		// IconDoll, freed when the Mail page went.
		case GIFT:      art = custom["IconDoll"]; break;
		case CHARACTER: art = custom["IconCharacter"]; break;
		case ADVENTURE: art = custom["IconAdventure"]; break;
		case INVENTORY: art = custom["IconInventory"]; break;
		case EQUIPMENT: art = custom["IconEquipment"]; break;
		case EQ_GEAR:   art = custom["IconWorn"]; break;
		case EQ_CASH:   art = custom["IconCosmetic"]; break;
		case EQ_PET:    art = custom["IconTabPet"]; break;
		case ABILITY:   art = custom["IconCharInfo"]; break;
		case QUESTS:    art = custom["IconQuest"]; break;
		case HOTKEYS:   art = custom["IconHotkeys"]; break;
		case CHAT:      art = custom["IconSocial"]; break;
		case WORLDMAP:  art = custom["IconMap"]; break;
		case MINIMAP:   art = custom["IconScroll"]; break;
		case SETTINGS:  art = custom["IconSettings"]; break;
		case OPTIONS:   art = custom["IconSettings"]; break;
		// The gamepad, which is what it always looked like.
		case CONTROLLER: art = custom["IconMinigame"]; break;
		case REPORT:    art = custom["IconReport"]; break;
		case KEYBINDS:  art = custom["IconKeyBindings"]; break;
		case EXITGAME:  art = custom["IconExit"]; break;
		case PARTY:     art = custom["IconParty"]; break;
		case INV_EQUIP: art = custom["IconTabEquip"]; break;
		case INV_USE:   art = custom["IconTabUse"]; break;
		case INV_SETUP: art = custom["IconScroll"]; break;
		case INV_ETC:   art = custom["IconTabEtc"]; break;
		case INV_CASH:  art = custom["IconTabCash"]; break;
		case DAILY:     art = custom["IconDaily"]; break;
		case PVE:       art = custom["IconPvE"]; break;
		case PVP:       art = custom["IconPvP"]; break;
		case SKILLS:    art = custom["IconSkills"]; break;
		case EMOTIONS:  art = custom["IconEmotions"]; break;
		case SHOUT:     art = custom["IconShout"]; break;
		case ROOMCHAT:  art = custom["IconRoomMessage"]; break;
		case SAY:       art = custom["IconChat"]; break;
		// The trade icon on the page that STARTS one, because that is the
		// one in the menu and "Trade" is what a player is looking for. The
		// table itself only ever appears in the breadcrumb.
		case NEARBY:    art = custom["IconTrade"]; break;
		case TRADE:     art = custom["IconTrade"]; break;
		case STORAGE:   art = custom["IconSave"]; break;
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
		case EQ_GEAR:   return "Worn";
		case EQ_CASH:   return "Cosmetic";
		case EQ_PET:    return "Pet";
		case ABILITY:   return "Stats";
		case QUESTS:    return "Quests";
		// WORLDMAP was missing entirely, so the Map button drew no label at
		// all while every other button had one.
		case WORLDMAP:  return "World";
		case MINIMAP:   return "Map";
		case HOTKEYS:   return "Hotkeys";
		case CHAT:      return "Social";
		case CASHSHOP: return "Cash Shop";
		case SETTINGS:   return "Settings";
		case OPTIONS:    return "Options";
		case CONTROLLER: return "Controller";
		case REPORT:     return "Report";
		case KEYBINDS:   return "Keys";
		case DAILY:      return "Daily";
		case PVE:        return "PvE";
		case PVP:        return "PvP";
		case SKILLS:     return "Skills";
		case EMOTIONS:   return "Emotions";
		case PARTY:      return "Party";
		case INV_EQUIP:  return "Equip";
		case INV_USE:    return "Use";
		case INV_SETUP:  return "Setup";
		case INV_ETC:    return "Etc";
		case INV_CASH:   return "Cash";
		case EXITGAME:   return "Quit";
		case SHOUT:     return "Voice";
		case ROOMCHAT:  return "Message";
		case SAY:       return "Chat";
		case NEARBY:    return "Trade";
		case GIFT:      return "Gifts";
		case TRADE:     return "Trading";
		case STORAGE:   return "Storage";
		default:        return nullptr;
		}
	}

	bool SecondScreenPanel::hotkey_jump_visible() const
	{
		// Only where there is something to carry away.
		// The pages you would be carrying something AWAY from.
		// THE LEAVES, NOT THE FOLDER. EQUIPMENT stopped being a page and
		// became a menu of three, so the button sat on a screen of buttons
		// where nothing can be picked up - and was missing from the three
		// pages where something actually can be.
		return current == INV_EQUIP || current == INV_USE
			|| current == INV_SETUP || current == INV_ETC || current == INV_CASH
			|| current == EQ_GEAR || current == EQ_CASH || current == EQ_PET
			|| current == SKILLS;
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
		//
		// ABOVE THE NUMBERS, not on top of them. Sitting 10 from the bottom
		// put it across the MP figure, which draw_vitals puts at
		// (VITAL_W + 17) up from the foot - the button covered the one thing
		// on that side of the panel you might be reading while deciding what
		// to put on a key. Measured off the same two constants, so the two
		// cannot drift apart again.
		constexpr int16_t NUMBERS = VITAL_W + 17;
		constexpr int16_t CLEAR = 6;

		int16_t bottom = static_cast<int16_t>(screen.y() - NUMBERS - CLEAR);

		return Rectangle<int16_t>(
			Point<int16_t>(screen.x() - W - 10, static_cast<int16_t>(bottom - H)),
			Point<int16_t>(screen.x() - 10, bottom));
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

		// NO PARCHMENT ANYWHERE. Tried on the leaf windows and then on Home
		// as well, and it was worse than the frame it covered: the panel's own
		// moving background is what makes this look like the game rather than
		// like a form, and a sheet of paper over it threw that away for a bit
		// of contrast the windows did not actually need.

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
				// ABOVE THE KEYS, AND SIZED TO WHAT IS LEFT.
				//
				// It used to be drawn at a fixed y 20 at its full width, from
				// when the keyboard was half the screen and the top half was
				// empty. The keyboard reaches higher now and cut the wordmark
				// in half - the first thing anybody sees on this screen, with
				// its bottom missing.
				Point<int16_t> size = logo.get_dimensions();

				int16_t room = static_cast<int16_t>(keyboard_top(screen) - 20);

				// Never wider than the panel, never taller than the gap. The
				// smaller of the two scales wins, so it always fits whichever
				// way it is short.
				double fit = std::min(
					static_cast<double>(screen.x() - 40) / size.x(),
					static_cast<double>(room - 12) / size.y());

				if (fit > 1.0)
					fit = 1.0;

				Point<int16_t> to(
					static_cast<int16_t>(size.x() * fit),
					static_cast<int16_t>(size.y() * fit));

				logo.draw(DrawArgument(
					Point<int16_t>(
						static_cast<int16_t>((screen.x() - to.x()) / 2),
						static_cast<int16_t>((room - to.y()) / 2 + 6)),
					to));
			}

			// AND THE KEYBOARD, WHICH IS THE WHOLE POINT OF THIS SCREEN AT
			// THE LOGIN.
			//
			// Drawn here rather than at the bottom of this function, because
			// the return below it means nothing after this branch ever runs
			// while the game is not started - which is exactly when somebody
			// is typing an account name.
			if (keyboard_wanted())
				draw_keyboard(screen);

			return;
		}

		// A GUEST IS DRAWN AFTER THE CHROME, not before it - see the bottom
		// of this function. A shop arrived under the gauges, the address bar
		// and the TO HOTKEYS button, because everything the panel owns is
		// drawn by draw_chrome and draw_chrome runs after the page.
		if (element && !has_guest())
		{
			Point<int16_t> at = window_position(screen);

			element->set_position(at);
			element->draw(1.0f);
		}
		else if (has_guest())
		{
			// Nothing here. The guest goes on last.
		}
		else if (current == OPTIONS)
		{
			draw_options(screen);
		}
		else if (current == EMOTIONS)
		{
			draw_emotions(screen);
		}
		else if (current == REPORT)
		{
			draw_report(screen);
		}
		else if (current == PVE)
		{
			draw_daily(screen);
		}
		else if (current == SHOUT)
		{
			draw_voice(screen);
		}
		else if (current == SAY)
			draw_chat(screen);
		else if (current == NEARBY)
			draw_nearby(screen);
		else if (current == GIFT)
			draw_gift(screen);
		else if (current == ROOMCHAT)
		{
			draw_messages(screen);
		}
		else if (current == PARTY)
		{
			draw_party(screen);
		}
		else if (current == KEYBINDS)
		{
			draw_keys(screen);
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

		// THE KEYBOARD, OVER EVERYTHING THE PANEL OWNS.
		//
		// Not a page: it appears because somebody needs to type, and while it
		// is up it is the only thing on the lower screen worth touching.
		if (keyboard_wanted())
			draw_keyboard(screen);

		// THE GUEST, ON TOP OF EVERYTHING THE PANEL OWNS.
		//
		// It is not a page of this panel - it is a window that arrived because
		// an NPC opened it, it fills the screen while it is up, and reading it
		// through the panel's own furniture is exactly the wrong way round.
		if (element && has_guest())
		{
			Point<int16_t> at = window_position(screen);

			element->set_position(at);
			element->draw(1.0f);
		}

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
