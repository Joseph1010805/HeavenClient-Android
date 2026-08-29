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

#include "../UIElement.h"

#include "../Components/Charset.h"
#include "../Components/Gauge.h"
#include "../Character/CharStats.h"
#include "../Graphics/SpecialText.h"

namespace ms
{
	class UIStatusbar : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::STATUSBAR;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		enum MenuType
		{
			MENU,
			SETTING,
			COMMUNITY,
			CHARACTER,
			EVENT
		};

		UIStatusbar(const CharStats& stats);

		void draw(float alpha) const override;
		void update() override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		void toggle_qs();
		void toggle_menu();
		void remove_menus();
		bool is_menu_active();

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		static constexpr int16_t QUICKSLOT_MAX = 211;

		float getexppercent() const;
		float gethppercent() const;
		float getmppercent() const;

		void toggle_qs(bool quick_slot_active);
		void toggle_setting();
		void toggle_community();
		void toggle_character();
		void toggle_event();
		void remove_active_menu(MenuType type);

		Point<int16_t> get_quickslot_pos();

		// The quickslot bar has twelve empty cells and nothing has ever put
		// anything in them - it is drawn as two flat textures. On a handheld
		// they line up with the twelve pad buttons, so each cell is labelled
		// with its button and shows whatever that button's key is bound to in
		// the Key Bindings window.
		//
		// The cells hold no bindings of their own - they are a view onto the
		// keymap, and dropping on one writes to that same keymap. So the bar
		// and the Key Bindings window can never disagree, and a binding made
		// either way is the one the server stores.
		static constexpr size_t QUICKSLOT_COLS = 6;
		static constexpr size_t QUICKSLOT_ROWS = 2;
		static constexpr size_t QUICKSLOT_COUNT = QUICKSLOT_COLS * QUICKSLOT_ROWS;

		void load_padslots();
		void draw_padslots(Point<int16_t> bar_pos) const;
		void draw_padslot_icon(const Texture& icon, Point<int16_t> cell) const;

		// Where the bar is drawn right now, which moves as it slides open and
		// shut. Both drawing and dropping measure from here.
		Point<int16_t> quickslot_bar_pos() const;

		// Which cell a point falls in, or -1. Only answers while the bar is
		// open, so a drop cannot land on cells that are slid off-screen.
		int16_t padslot_by_position(Point<int16_t> cursorpos) const;

		bool send_icon(const Icon& icon, Point<int16_t> cursorpos) override;

		// Writes a mapping into a quickslot cell, telling the server first.
		// Shared by dropping an icon on the bar and by tapping a cell while
		// the panel has something selected.
		bool bind_padslot(int16_t slot, Keyboard::Mapping mapping);

		struct PadSlot
		{
			OutlinedText label;
			int16_t keycode = -1;
		};

		PadSlot padslots[QUICKSLOT_COUNT];

		// Where the borrowed exit button sits relative to the setting menu.
		// x centres a 98-wide sprite in a 109-wide row; y is the fifth slot,
		// raised so a 35-tall sprite sits inside a 26-tall row.
		static constexpr Point<int16_t> SETTING_EXIT_OFFSET = { 11, 141 };

		enum Buttons : uint16_t
		{
			BT_CASHSHOP,
			BT_MENU,
			BT_OPTIONS,
			BT_CHARACTER,
			BT_COMMUNITY,
			BT_EVENT,
			BT_FOLD_QS,
			BT_EXTEND_QS,
			BT_MENU_QUEST,
			BT_MENU_MEDAL,
			BT_MENU_UNION,
			BT_MENU_MONSTER_COLLECTION,
			BT_MENU_AUCTION,
			BT_MENU_MONSTER_LIFE,
			BT_MENU_BATTLE,
			BT_MENU_ACHIEVEMENT,
			BT_MENU_FISHING,
			BT_MENU_HELP,
			BT_MENU_CLAIM,
			BT_SETTING_CHANNEL,
			BT_SETTING_OPTION,
			BT_SETTING_KEYS,
			BT_SETTING_JOYPAD,
			BT_SETTING_QUIT,
			// Must stay AFTER BT_SETTING_QUIT: the loops that reposition and
			// key-navigate the setting menu run up to that one, and this
			// button is placed by hand rather than by a baked-in origin.
			BT_SETTING_EXIT,
			BT_COMMUNITY_FRIENDS,
			BT_COMMUNITY_PARTY,
			BT_COMMUNITY_GUILD,
			BT_COMMUNITY_MAPLECHAT,
			BT_CHARACTER_INFO,
			BT_CHARACTER_STAT,
			BT_CHARACTER_SKILL,
			BT_CHARACTER_EQUIP,
			BT_CHARACTER_ITEM,
			BT_EVENT_SCHEDULE,
			BT_EVENT_DAILY,

			// SHOUT and SPEAK live on the main screen, always visible.
			//
			// They were on the chat bar first, which was wrong: those buttons
			// only exist while the chat is OPEN, so on a handheld where the
			// chat spends its life folded away they were invisible. A thing
			// you use to start talking cannot be hidden behind having already
			// started talking.
			//
			// Kept at the END of this enum deliberately - draw() runs a loop up
			// to BT_EVENT and the setting menu runs loops up to
			// BT_SETTING_QUIT, and both would swallow anything inserted higher.
			BT_SHOUT,
			BT_SPEAK
		};

		// Where SHOUT and SPEAK sit, worked out from the menu row itself so
		// they stay with it at every screen width.
		Point<int16_t> shout_pos;
		Point<int16_t> speak_pos;

		// One size for both, matching a menu button (34x37 artwork).
		//
		// NOT called ICON_SIZE: draw_padslots has a function-local constant of
		// that name for the quickslot icons, and a member with the same name
		// would sit there shadowed and waiting to change their size the day
		// somebody deleted the local one.
		static constexpr int16_t EXTRA_ICON_SIZE = 42;

		// How far above the bottom edge the status bar starts. The same at
		// every resolution the client has ever shipped - see the constructor.
		static constexpr int16_t BAR_FROM_BOTTOM = 120;

		// How much bigger the menu row is drawn, and the extra pitch that
		// needs. The artwork is 34 wide on a 35 pitch, so at 1.4x each button
		// grows by about 14 and the gap has to grow with it.
		// HOW MUCH BIGGER THE MENU ROW IS DRAWN.
		//
		// The artwork is 34x37, sized for a mouse. These are pressed with a
		// thumb, and a bitmap has no larger version to load, so scaling the
		// picture is the only lever.
		static constexpr float MENU_SCALE = 1.25f;

		// Where each button's LEFT EDGE goes, and how far apart.
		//
		// The row cannot be laid out by scaling alone. Every one of these
		// buttons is created at the SAME position and spread out by an origin
		// baked into its bitmap (Menu is -174, Setting -139, Character -69) -
		// so scaling multiplies the origin as well and they fly apart by the
		// same factor. The fix is to place them explicitly and cancel the
		// origin back out:
		//
		//     position = wanted_left + origin * MENU_SCALE
		//
		// Right-aligned to the screen edge and walked leftwards, so the row
		// keeps its shape whatever the scale is set to.
		static constexpr int16_t MENU_RIGHT = 798;
		static constexpr int16_t MENU_PITCH = 45;

		// HOW FAR LEFT THE HP/MP PANEL MOVES.
		//
		// It sat directly against the button row, which left 208 pixels for six
		// controls - exactly what they need at their original size and not a
		// pixel more. No scale fits in that, so the panel moves and the row
		// gets the room.
		static constexpr int16_t HPMP_SHIFT = 150;

		Texture shout_icon;
		Texture speak_icon;

		const CharStats& stats;

		Gauge expbar;
		Gauge hpbar;
		Gauge mpbar;
		Charset statset;
		Charset hpmpset;
		Charset levelset;
		Texture quickslot[2];
		Texture menutitle[5];
		Texture menubackground[3];
		OutlinedText namelabel;
		std::vector<Sprite> hpmp_sprites;

		Point<int16_t> exp_pos;
		Point<int16_t> hpmp_pos;
		Point<int16_t> hpset_pos;
		Point<int16_t> mpset_pos;
		Point<int16_t> statset_pos;
		Point<int16_t> levelset_pos;
		Point<int16_t> namelabel_pos;
		Point<int16_t> quickslot_pos;
		Point<int16_t> quickslot_adj;
		Point<int16_t> quickslot_qs_adj;
		Point<int16_t> menu_pos;
		Point<int16_t> setting_pos;
		Point<int16_t> community_pos;
		Point<int16_t> character_pos;
		Point<int16_t> event_pos;
		int16_t quickslot_min;
		int16_t position_x;
		int16_t position_y;

		bool quickslot_active;
		int16_t VWIDTH;
		int16_t VHEIGHT;

		bool menu_active;
		bool setting_active;
		bool community_active;
		bool character_active;
		bool event_active;
	};
}