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

			// The game's own option menu, and the minigames. Minigames has no
			// window behind it yet and shows as an empty page.
			SETTINGS,
			MINIGAME,

			// SOCIAL is a menu too: talking to the room, and talking OUT LOUD
			// to it. The mail is the room's message board; the megaphone is
			// voice.
			SHOUT,
			ROOMCHAT,

			WORLDMAP,
			// Named for the game's own Character menu tabs, so the panel and the
			// top screen call the same things by the same names.
			INVENTORY,
			EQUIPMENT,
			// Stats, with skills beneath them - see UICharacterPage.
			ABILITY,
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

		// Navigation. go_to descends, go_back climbs one level, go_home
		// returns to the root.
		void go_to(Page which);
		void go_back();
		void go_home();

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

		// The panel's own background, under the page rather than over it.
		void draw_frame(Point<int16_t> screen) const;

		// The settings list. Names and values, no controls yet.
		void draw_options(Point<int16_t> screen) const;

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
		mutable Texture hp_icon;
		mutable Texture mp_icon;
		mutable Texture clock_icon;

		mutable Text clock_text;

		// The name of the page you are ON, drawn after the last crumb.
		mutable Text crumb_label;

		// The options list on the Settings page. Names only for now - the
		// artwork for them has not been chosen, and a row of placeholder
		// icons would be harder to replace than a row of words.
		mutable Text option_text;

		// A vertical drag scrolls the page under it. Held between calls
		// because send_touch is given a position, not a delta.
		mutable int16_t drag_y = 0;
		mutable bool dragging = false;

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
		void draw_art(const Texture& art, Rectangle<int16_t> box, int16_t pad,
			int16_t lift = 0) const;
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
