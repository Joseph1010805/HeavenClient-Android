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
#include <vector>

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
		// BUTTONS WITHIN BUTTONS, not tabs within tabs.
		//
		// The panel is a TREE, not a deck of pages you swipe through. HOME and
		// CHARACTER are menus - a screenful of big buttons, nothing else -
		// and everything below them is a leaf that hosts a real window.
		//
		// You go IN by pressing, BACK by swiping left-to-right, and HOME by
		// swiping right-to-left. Nothing to aim at, no page dots to count, and
		// no way to be somewhere without knowing how you got there.
		enum Page
		{
			HOME,
			CHARACTER,

			// Where am I going next - the world map and the quest log, which
			// both answer that one question. A MENU now, not a leaf.
			ADVENTURE,

			// SETTINGS is a menu: the list of options, the pad, and a way to
			// report a bug. MINIGAME's controller artwork moved to CONTROLLER,
			// which is what a picture of a gamepad should have meant all along.
			SETTINGS,
			OPTIONS,
			KEYBINDS,
			CONTROLLER,
			REPORT,
			EXITGAME,

			// DAILY is what used to be the Minigames button on Home: the
			// things you come back for each day, split by who you fight.
			DAILY,
			PVE,
			PVP,

			// SOCIAL is a menu too: talking to the room, and talking OUT LOUD
			// to it. The mail is the room's message board; the megaphone is
			// voice.
			SHOUT,
			// SAY is TYPING; ROOMCHAT is READING.
			//
			// They were one page and it did not work. Reading wants the whole
			// screen and a scroll; typing wants a keyboard over most of it,
			// and the keyboard was drawn straight across the log and the
			// SPEAK button underneath. Splitting them lets each have the
			// screen it needs, and it is the honest split anyway - looking at
			// what was said and saying something are different errands.
			SAY,

			ROOMCHAT,

			// DUEY'S COUNTER. Leaving something for somebody who is not here,
			// and collecting what was left for you.
			//
			// Distinct from TRADE, which is hand to hand and needs both of you
			// standing in the same place at the same time. This one waits.
			GIFT,

			EMOTIONS,
			PARTY,

			// WHO IS STANDING HERE, and a tap to ask one of them to trade.
			//
			// The original trades by right-clicking somebody's character on
			// the map. There is no right button here and the characters are
			// on the OTHER screen, so the people in the room are a list.
			NEARBY,

			// The table itself. Reached by pressing a name, or by a trade
			// opening on its own because somebody asked YOU.
			TRADE,

			// The account's bank. Only ever reached by an NPC opening it.
			STORAGE,

			WORLDMAP,

			// THIS map, rather than the world. Where you are standing, who is
			// near you, and which way the quest is.
			MINIMAP,
			// Named for the game's own Character menu tabs, so the panel and the
			// top screen call the same things by the same names.
			// A FOLDER, not a page with a tab strip.
			//
			// Five buttons that take you INTO a section, the way Character and
			// Adventure do. A row of tabs inside one page was the panel
			// growing a second navigation system beside the one it has.
			INVENTORY,
			INV_EQUIP,
			INV_USE,
			INV_SETUP,
			INV_ETC,
			INV_CASH,

			EQUIPMENT,

			// THE EQUIPMENT WINDOW'S THREE TABS, AS PAGES.
			//
			// The inventory's five sections stopped being tabs inside a
			// window and became buttons on the panel - one page each, reached
			// through Character > Inventory. The equipment window kept its
			// own tab strip along the top, so the same three things worked
			// two different ways depending on which window you were in.
			//
			// These make it one way. EQUIPMENT is now a menu, like INVENTORY.
			EQ_GEAR,
			EQ_CASH,
			EQ_PET,
			// STATS ALONE. Skills were merged in beneath them and are their own
			// page again: one scrolling column holding two unrelated windows
			// meant neither could be laid out for the space it had, and the
			// skill page's action bar spent three sessions below the screen.
			ABILITY,
			SKILLS,
			QUESTS,
			HOTKEYS,
			CHAT,

			// THE CASH SHOP, on Home.
			//
			// An ACTION, not a page - see menu_action. It has no panel window of
			// its own; pressing it asks the server to move the character into the
			// shop, which takes over the whole client. It lives here because on a
			// one-screen device the game's own Cash Shop button is gone with the
			// rest of that bottom row, and this is the only way in.
			CASHSHOP,

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

		// Scroll the page that is showing.
		//
		// Handed straight to the game window behind the page, because these
		// ARE the game's own windows - the inventory, the skill book and the
		// quest log each already own a slider, and this is the same signal a
		// mouse wheel would give them. Nothing new to keep in step.
		void send_scroll(double yoffset);

		Page page() const;

		// Turn to a page by name. Used by the HOTKEY buttons on the item,
		// equipment and skill pages, which take you to the slots rather than
		// making you swipe there while holding a selection.
		void show_page(Page which);

		// Turn to the trade page and BUILD its window if it does not exist -
		// the first thing that happens in a trade is a packet, before anybody
		// has looked at the page. See SecondScreen::open_trade.
		UIElement* open_trade();
		UIElement* open_storage();

		// Navigation. go_to descends, go_back climbs one level, go_home
		// returns to the root.
		void go_to(Page which);
		void go_back();
		void go_home();

		// SOMEBODY SWIPED BACK WITH NOWHERE LEFT TO GO. On a real panel that
		// is nothing - the panel is always on that screen and the root is as
		// far out as it goes. On the one-screen overlay it is the way out:
		// back from the top means put the whole thing away, which is what the
		// same gesture does everywhere else on a handheld.
		//
		// Read and cleared, so it is acted on once.
		bool take_back_at_root();

		// DOES ANYTHING IN HERE WANT ATTENTION? The same badge the menu
		// already draws on its own buttons, rolled up into one answer so a
		// single MENU icon on a one-screen device can carry it.
		bool any_alert() const;

		// A GUEST: a window the panel shows without owning.
		//
		// A shop is not a page - you cannot swipe to it, it appears because an
		// NPC opened it and leaves when it closes. It also belongs to UI, which
		// created it, so the panel must not take it into `pages` and delete it.
		// While a guest is up it replaces the page entirely: it is drawn, it
		// takes the touches and it takes the scroll.
		void show_guest(UIElement* element, const char* backdrop);
		void clear_guest();
		bool has_guest() const { return guest != nullptr; }

		// The hosted window of this type, if one has been built. Null
		// otherwise - this never builds one, it only reports.
		UIElement* hosted(UIElement::Type type) const;

		// What the current page has picked out, as something a key could be
		// bound to. NONE when nothing is selected.
		Keyboard::Mapping selected_mapping() const;

		// The last thing any page had picked out, which SURVIVES a page turn.
		//
		// selected_mapping() only ever answers for the page showing now, so it
		// goes blank the moment you leave the item page - and picking a potion
		// up on one page to put it down on another is the whole point of the
		// hotkey page.
		Keyboard::Mapping carried_mapping() const { return carried; }

		// PUT IT DOWN.
		//
		// Placing something in a hotkey slot has to end the carry, or every
		// later tap on the page takes the "you are holding something" branch
		// and drops the same potion into a different slot. The page could
		// then be filled, and nothing on it could ever be USED.
		void clear_carried() { carried = Keyboard::Mapping(); }

		// Whether the pointer is on the panel rather than the main screen.
		bool has_cursor() const { return cursor_here; }

	private:
		void turn_to(int16_t next);

		// Forget whatever the page we are leaving put on screen.
		void leave_page();

		// A menu is a page made ONLY of buttons - it hosts no window.
		static bool is_menu(Page page);
		// What a menu offers, in the order it is drawn.
		static const Page* menu_items(Page page, size_t& count);

		void draw_menu(Point<int16_t> screen) const;

		// SOMETHING IS WAITING BEHIND THIS BUTTON.
		//
		// A small badge over the corner of a menu icon. The panel is a tree,
		// so a level-up to spend or a daily still to do is two or three
		// presses down and invisible from the top - which is how points go
		// unspent for a whole evening. The badge is drawn on the way in as
		// well as on the page itself.
		bool page_alert(Page page) const;

		mutable Texture alert_badge;
		mutable bool alert_tried = false;

		// THE ONE BOX EVERYTHING HAS TO FIT IN.
		//
		// Four things own the edges of this panel and are drawn AFTER every
		// page, so anything that strays under them is simply lost: the HP
		// gauge down the left, MP down the right, the EXP bar along the
		// bottom with its three numbers just above it, and the address bar
		// and clock across the top.
		//
		// Every page was laid out against the raw screen instead, so each one
		// had its own idea of where the edges were and most of them were
		// wrong. This is the single answer. A page that asks gets a rectangle
		// it can use ALL of, and hosted windows are told this size rather
		// than the screen's so they lay themselves out inside it.
		Rectangle<int16_t> content_area(Point<int16_t> screen) const;

		// The panel's own background, under the page rather than over it.
		void draw_frame(Point<int16_t> screen) const;

		// The settings list. Names and values, no controls yet.
		void draw_options(Point<int16_t> screen) const;

		// SOME BUTTONS ARE ACTIONS, NOT PLACES.
		//
		// Quit was wired as a page: pressing it navigated to an empty screen
		// and the dialog only appeared if you tapped AGAIN once you were
		// there. From the outside that is a button that does nothing, which
		// is exactly what it was reported as - twice, because the first fix
		// improved the dialog without noticing nobody ever reached it.
		//
		// True when the press was handled here and must not navigate.
		bool menu_action(Page which);

		// THE KEYBOARD.
		//
		// None of these devices has one, and the two places that need typing
		// - the login form and the chat - were both unreachable without
		// Android's own soft keyboard, which appears over the game, covers
		// the thing being typed into, and has no idea where the panel is.
		//
		// This fills the lower screen instead, which is exactly what a lower
		// screen is for. It emits through UI::send_key, so a tapped letter
		// takes the identical path a physical key would: whatever has focus
		// receives it and nothing here needs to know what that is.
		bool keyboard_wanted() const;

		void draw_keyboard(Point<int16_t> screen) const;
		bool keyboard_pressed(Point<int16_t> at, Point<int16_t> screen);

		Rectangle<int16_t> key_cap_box(size_t row, size_t col,
			Point<int16_t> screen) const;

		// Where the keys begin. Lower at the login, where the panel has
		// nothing else on it and the keys can afford to be big; higher in
		// game, where the chat above it has to stay readable.
		int16_t keyboard_top(Point<int16_t> screen) const;

		// Shift is a LATCH, not a hold: one thumb, and holding a modifier
		// while reaching for a letter needs two.
		mutable bool kb_shift = false;
		mutable bool kb_symbols = false;

		mutable Text kb_text;

		// The parchment the keys sit on. Looked up once - a failed lookup is
		// as slow as a good one and this runs every frame.
		mutable Texture kb_paper;
		mutable bool kb_paper_tried = false;


		// THE KEYS PAGE.
		//
		// Every action the game can bind, what it is bound to now, and two
		// things to do with the one you pick: put it on a HOTKEY, or bind it
		// to a CONTROLLER button. Scrolls, because there are far more actions
		// than rows on a panel this size.
		void draw_keys(Point<int16_t> screen) const;
		static size_t bindable_count();
		Rectangle<int16_t> key_row_box(size_t row, Point<int16_t> screen) const;
		Rectangle<int16_t> key_hotkey_box(Point<int16_t> screen) const;
		Rectangle<int16_t> key_bind_box(Point<int16_t> screen) const;
		bool keys_pressed(Point<int16_t> at, Point<int16_t> screen);

		// Which action is picked out, as an index into the table, and how far
		// the list has been scrolled.
		mutable int16_t key_selected = -1;
		mutable int16_t key_scroll = 0;

		// WAITING FOR A BUTTON.
		//
		// While this is set the next controller press is captured and bound
		// rather than acted on. It is a mode, and a mode you cannot see is a
		// trap - so the page says so in large letters while it is on.
		mutable bool key_binding = false;

		mutable Text key_text;
		mutable Text key_label;

		// THE PARTY PAGE: who is with you, and the buttons that change that.
		void draw_party(Point<int16_t> screen) const;
		Rectangle<int16_t> party_action_box(Point<int16_t> screen) const;

		mutable Text party_text;
		mutable Text party_head;

		// THE CHAT PAGE: the room, as balloons, with a keyboard under it.
		//
		// The same log the Messages page reads, drawn as speech rather than
		// as a transcript - which is what it is, and what makes a glance at
		// it tell you who is talking rather than needing to be read.
		//
		// The line being typed does NOT go through a Textfield. The panel's
		// keyboard would have to reach across and focus the chat window's own
		// field on the other screen, and a field that has focus swallows
		// every key the game would otherwise get. This holds the characters
		// itself and hands the finished line to UIChatbar::say.
		void draw_chat(Point<int16_t> screen) const;

		// --- the post box ----------------------------------------------------


		// The row a message is drawn on, so a tap can find it.
		Rectangle<int16_t> message_row(int16_t index, Point<int16_t> screen) const;

		// THE MAIL PAGE IS GONE - see the note on the social menu. Its
		// write/to/body/send boxes, its pick-list and its keyboard went with
		// it. The post box underneath still runs; it delivers into the chat
		// log instead of into a page of its own.
		Rectangle<int16_t> say_box(Point<int16_t> screen) const;

		std::string say_line;

		mutable Text bubble_text;
		mutable Text say_text;

		// THE NEARBY PAGE: everybody on this map, one row each.
		void draw_nearby(Point<int16_t> screen) const;

		// Duey's counter - what is waiting, and who it can be sent to.
		void draw_gift(Point<int16_t> screen) const;

		// The rows on that page. Parcels first, then the people.
		Rectangle<int16_t> gift_parcel_row(size_t index, Point<int16_t> screen) const;
		Rectangle<int16_t> gift_name_row(size_t index, Point<int16_t> screen) const;

		// Where the second half of the page starts, measured off how many
		// parcels are waiting - so the list of people moves down as parcels
		// arrive rather than being drawn over.
		int16_t gift_split(Point<int16_t> screen) const;

		// A parcel row and a name row are the same height, so the two halves
		// of the page read as one list with a rule through it.
		static constexpr int16_t GIFT_ROW_H = 38;

		// A heading's worth of room above a list, and a line's worth kept
		// clear at the foot for whatever the server last said. Both were
		// eyeballed and both were too small, which is how the names ended up
		// drawn over the reply.
		static constexpr int16_t HEAD_H = 22;
		static constexpr int16_t FOOT_H = 18;

		// HOW FAR OUR OWN LISTS ARE SCROLLED, in pixels.
		//
		// The pages that are the game's own windows scroll themselves - they
		// each have a slider and send_scroll hands the wheel straight to
		// them. The lists this panel draws by hand had nothing: a party of
		// six, or a gift list of everybody you have played with, simply ran
		// off the bottom of the page with no way to reach the rest.
		//
		// One offset, reset on every page change, because only one of these
		// pages is ever on screen.
		int16_t list_scroll = 0;

		// How far it is allowed to go, worked out by the page that drew last.
		// Written while drawing because that is the only place that knows how
		// tall the content turned out to be.
		mutable int16_t list_scroll_max = 0;

		// Drag the list rather than the page. Returns true if it took the
		// movement, so a vertical drag on a list does not also swipe out.
		bool list_drag(Point<int16_t> position);

		// Draw the thumb, if there is anything to scroll. Nothing is drawn
		// when everything fits, so a short list has no furniture.
		void draw_scrollbar(Rectangle<int16_t> box, int16_t content) const;

		mutable Text gift_text;
		mutable Text gift_small;
		Rectangle<int16_t> nearby_row(int16_t index, Point<int16_t> screen) const;

		// Character ids, in the order the rows are drawn. Rebuilt on a beat -
		// people walk in and out - and held so that a tap lands on the person
		// who was actually under the thumb.
		mutable std::vector<int32_t> nearby;
		mutable std::vector<std::string> nearby_names;
		mutable int16_t until_nearby = 1;

		mutable Text nearby_text;

		// THE MESSAGES PAGE: what has been said, and a way to say something.
		//
		// The chat window itself stays on the top screen - it is the game's
		// own, it is sized for it, and two copies of a scrolling log fighting
		// over one row counter is not worth the trouble. This is a READER
		// with one button on it, which is what the lower screen is for.
		void draw_messages(Point<int16_t> screen) const;
		Rectangle<int16_t> speak_box(Point<int16_t> screen) const;

		// Who the next spoken message is addressed to. Empty until a name is
		// tapped, which is what stops SPEAK doing anything.
		std::string message_to;

		mutable Text message_text;
		mutable Text speak_text;

		// THE VOICE PAGE: one big button you hold to talk.
		//
		// Held, not latched - see Voice.h. The button is most of the page
		// because it is pressed with a thumb on a screen at arm's length,
		// while looking at the fight on the other screen and not at this.
		void draw_voice(Point<int16_t> screen) const;
		Rectangle<int16_t> talk_box(Point<int16_t> screen) const;

		mutable Text voice_text;
		mutable Text voice_hint;

		// THE ADDRESS BAR, top left: home, then the way in, then here.
		// Every step is a button back to that folder.
		size_t crumb_count() const;
		Page crumb_page(size_t index) const;
		Rectangle<int16_t> crumb_box(size_t index) const;
		Rectangle<int16_t> menu_box(size_t index, size_t count, Point<int16_t> screen) const;
		// Which menu button a point is on, or -1.
		int16_t menu_at(Point<int16_t> position, Point<int16_t> screen) const;

		// The heading and the row of dots, so it is always clear which page
		// this is and how many there are.
		void draw_chrome(Point<int16_t> screen) const;

		// HP and MP down the outer margins, and the EXP bar under the heading.
		//
		// In the MARGINS deliberately: they are the only strip of the panel no
		// page ever draws in, so this can always be on screen without covering
		// anything. Only a window in front of the panel hides it.
		void draw_vitals(Point<int16_t> screen) const;

		mutable Text hp_text;
		mutable Text mp_text;
		mutable Text exp_text;

		// Ticks up forever; the pulse is a function of it and of how low the
		// bar is, so a bar close to empty flashes faster than one half full.
		mutable uint32_t pulse = 0;

		// ONE THICKNESS FOR ALL FOUR MARGINS.
		//
		// HP left, MP right, the clock strip along the top and the EXP bar
		// along the bottom - all VITAL_W, so the page's backdrop is inset by
		// the same amount whichever edge you measure from. Uneven margins are
		// what made the panel look assembled rather than laid out.
		//
		// 12, down from 20 on the sides and 30 on the EXP bar. Four-sided
		// HP/MP bars at 40 were tried on 31 Aug and reverted the same day:
		// they took a quarter of the panel and read as a slab.
		static constexpr int16_t VITAL_W = 12;

		// The gauge's TROUGH, from artwork, stretched to fit.
		void channel(const Texture& art,
			int16_t x, int16_t y, int16_t w, int16_t h) const;

		mutable Texture bar_v;
		mutable Texture bar_h;

		// One rounded bar. Every gauge on the panel goes through this, so the
		// corners cannot drift apart between them.
		void bar(int16_t x, int16_t y, int16_t w, int16_t h,
			float r, float g, float b, float a) const;

		// The little potions and the clock, drawn at the head of their own
		// margins. Loaded on first use, not in the constructor - the panel is
		// built before the NX files are open.
		// hp_icon / mp_icon removed with the potion pips - see draw_vitals.
		mutable Texture clock_icon;

		mutable Text clock_text;

		// The name of the page you are ON, drawn after the last crumb.
		mutable Text crumb_label;

		// THE EMOTIONS PAGE: a grid of faces, each one sent when tapped.
		void draw_emotions(Point<int16_t> screen) const;
		Rectangle<int16_t> emotion_box(size_t index, Point<int16_t> screen) const;
		int16_t emotion_at(Point<int16_t> at, Point<int16_t> screen) const;

		// THE REPORT PAGE: writes what went wrong to a file that can be sent.
		void draw_report(Point<int16_t> screen) const;
		Rectangle<int16_t> report_box(Point<int16_t> screen) const;
		void write_report() const;

		mutable Text emotion_text;

		// No stored artwork: the buttons draw the character's OWN face in
		// each expression, straight from the Face the look already holds.
		mutable Text report_text;

		// The daily hunt page: the running count and the three tiers.
		void draw_daily(Point<int16_t> screen) const;

		// The START / countdown strip along the foot of the PvE page.
		Rectangle<int16_t> rush_box(Point<int16_t> screen) const;

		mutable Text daily_text;
		mutable Text daily_count;
		mutable std::string report_state;
		mutable int32_t report_count = 0;

		// The options list on the Settings page. Names only for now - the
		// artwork for them has not been chosen, and a row of placeholder
		// icons would be harder to replace than a row of words.
		mutable Text option_text;

		// A vertical drag scrolls the page under it. Held between calls
		// because send_touch is given a position, not a delta.
		mutable int16_t drag_y = 0;
		mutable bool dragging = false;

		// HOW LONG THE FINGER HAS BEEN DOWN, in frames.
		//
		// A swipe and a drag are the same shape; what tells them apart is
		// SPEED. On a map, a slow drag is somebody looking around and a quick
		// flick is somebody leaving - and reading the slow one as "go back"
		// is what threw people out of the map they were reading.
		mutable uint16_t touch_ticks = 0;

		// The press that opened a page must not also act ON that page.
		//
		// TO HOTKEYS navigates on the way DOWN; the matching UP then landed
		// on the hotkey grid and dropped the potion into whatever slot the
		// stylus happened to be over. One gesture, two meanings.
		mutable bool swallow_up = false;

		// HOME is only ever a small mark in the corner - it is the top of the
		// tree, so there is nowhere above it for a big button to lead.
		Rectangle<int16_t> home_box(Point<int16_t> screen) const;
		// Henesys' map mark IS a mushroom house - 38x38, drawn by the game on
		// its own world map. Map.nx/MapHelper.img/mark has 66 of these, all the
		// same size and all thematic; UI.nx has nothing like them.
		Texture home_icon;

		// The wooden notice frame the login and quit windows are built on, so
		// the panel's buttons belong to the same game as its dialogs.
		Texture tile_frame;

		mutable Text home_mark;

		// A shortcut to the hotkey page, shown only on the pages you would be
		// carrying something away from.
		bool hotkey_jump_visible() const;
		Rectangle<int16_t> hotkey_jump_box(Point<int16_t> screen) const;
		mutable Text hotkey_jump;

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

		// How we got here, root first. Empty means we are AT the root.
		std::vector<Page> trail;

		// Set by go_back when there is nowhere further out. See
		// take_back_at_root.
		bool back_at_root = false;

		// HOW LONG THE FINGER HAS BEEN DOWN, in frames.
		//
		// NOT touch_ticks. That only advances when a touch EVENT arrives, so
		// it measures how far a finger has travelled, not how long it has been
		// there - a finger held perfectly still generates no events at all and
		// never ages. Counted in update(), which runs every frame whether
		// anything moved or not.
		int16_t hold_ticks = 0;

		// About three quarters of a second at 60fps. Long enough that it
		// cannot happen while tapping, short enough not to feel broken.
		static constexpr int16_t HOLD_TO_CLEAR = 45;

		mutable Text menu_label;

		Keyboard::Mapping carried;

		// Not owned. Cleared the moment it stops being active - nothing tells
		// the panel a shop has closed, so it watches for it.
		UIElement* guest = nullptr;
		const char* guest_backdrop_name = nullptr;
		mutable Texture guest_backdrop_tex;

		std::unique_ptr<UIElement> pages[NUM_PAGES];

		// Where a drag started and whether one is in progress, which is all a
		// swipe is until the finger lifts.
		Point<int16_t> touch_start;

		// Where the pointer was last frame while it is held down, so a drag
		// can be handed over as a movement rather than as a position. See
		// UIElement::send_drag.
		Point<int16_t> drag_from;
		Point<int16_t> touch_now;
		// How far a finger may wander between going down and coming up and
		// still count as a tap rather than a drag. A finger is never quite
		// still, so zero would make taps unreliable.
		static constexpr int16_t DRAG_SLOP = 12;

		// How far sideways a finger must travel to count as a swipe rather
		// than a wandering tap. Comfortably above DRAG_SLOP so the two cannot
		// both be true of the same gesture.
		static constexpr int16_t SWIPE_MIN = 70;

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

		// The name of the page, across the top. The map is left alone - it is
		// unmistakable, and it uses every pixel it has.
		static const char* page_name(Page page);

		// Which of Map.nx's 66 map marks stands for a page, or null. These are
		// the game's own town icons - Henesys IS a mushroom house - so the
		// buttons are furnished from the game rather than from invented art.
		// The artwork for a page, as a Texture rather than a name - they come
		// from three different files and no single path pattern fits.
		Texture page_art(Page page) const;

		// Draw a texture INSIDE a box: centred, aspect kept, origin handled.
		//
		// The origin is the trap. DrawArgument::get_rectangle computes
		// `pos - center - origin`, so a texture drawn at a position lands at
		// position MINUS its origin - and a map object like the mushroom house
		// carries a large one, which throws it clean off the tile. UIStatusbar
		// adds the origin back for the same reason.
		// Returns the art's BOTTOM CENTRE, so a caller can hang a label under
		// it without repeating the scaling arithmetic.
		Point<int16_t> draw_art(const Texture& art, Rectangle<int16_t> box,
			int16_t pad, int16_t lift = 0) const;
		mutable std::map<int, Texture> art_cache;
		mutable Text page_title;

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
		mutable Texture logo;
		mutable bool logo_tried = false;
	};
}
