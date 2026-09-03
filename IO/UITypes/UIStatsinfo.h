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

#include "../UIDragElement.h"

#include "../Template/BoolPair.h"
#include "../Character/CharStats.h"
#include "../Graphics/Text.h"

namespace ms
{
	class UIStatsinfo : public UIDragElement<PosSTATS>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::STATSINFO;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIStatsinfo(const CharStats& stats);

		// Show this copy on the lower panel: pinned, with the window's own
		// close box and drag bar taken away.
		void set_panel(Point<int16_t> screen);

		// Needed only so the panel's own chips get the press before the
		// window's drag handling does anything with it.
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		void draw(float alpha) const override;
		void update() override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;

		UIElement::Type get_type() const override;

		void update_all_stats();
		void update_stat(Maplestat::Id stat);

	protected:
		bool indragrange(Point<int16_t> cursorpos) const override;

		bool panel = false;

		// The room this page has been given - the panel's CONTENT box, not
		// the screen, so the popup below can size itself to what is actually
		// usable rather than to pixels that are under a gauge.
		Point<int16_t> panel_screen = Point<int16_t>(344, 300);

		static constexpr float PANEL_FADE = 0.6f;


		Button::State button_pressed(uint16_t buttonid) override;

	private:
		enum StatLabel
		{
			// Normal
			NAME, JOB, GUILD, FAME, DAMAGE, HP, MP, AP, STR, DEX, INT, LUK, NUM_NORMAL,
			// Detailed
			DAMAGE_DETAILED, DAMAGE_BONUS, BOSS_DAMAGE, FINAL_DAMAGE, IGNORE_DEFENSE, CRITICAL_RATE, CRITICAL_DAMAGE, STATUS_RESISTANCE, KNOCKBACK_RESISTANCE, DEFENSE, SPEED, JUMP, HONOR,
			// Total
			NUM_LABELS
		};

		// The panel draws its own compact list instead of the window's.
		//
		// The artwork is 212 across and the DETAIL column that opens beside it
		// is another 213 - 425 on a panel 344 wide, so the detail was always
		// going to be off the screen. Every row of that artwork is mostly
		// blank, so the list is drawn here at 130 across and DETAIL moves in to
		// meet it: 130 + 213 is 343, which fits with a pixel to spare.
		// The page is a two-column sheet: who the character is and what can be
		// spent on the left, everything derived from it on the right. Nothing
		// is behind a button - the Detail toggle is gone and all of it shows at
		// once, which is the whole point of a screen that is always open.
		// 13, not 15. Thirteen rows at 15 plus a 26px header is 221, and the
		// panel's content box is about 236 tall - which left nothing for the
		// LEVEL UP button underneath and put it over the HP numbers. At 13
		// the sheet ends near 195 and the button has its own room.
		static constexpr int16_t PANEL_ROW_H = 13;
		static constexpr int16_t PANEL_TOP = 26;
		static constexpr int16_t PANEL_COL_W = 166;
		// Clear of the HP gauge down the left edge, which is drawn after the
		// page and was eating the first letter of every row.
		static constexpr int16_t PANEL_LEFT_X = 16;
		static constexpr int16_t PANEL_RIGHT_X = 176;

		// Where a value sits within its column, and where the spend arrow
		// after it goes.
		static constexpr int16_t PANEL_VALUE_X = 54;
		static constexpr int16_t PANEL_ARROW_X = 148;

		// THE PANEL'S OWN SPEND CONTROLS.
		//
		// The window's real buttons are MapleButtons placed against artwork
		// that the panel does not draw. They were still active and still
		// taking touches from wherever their sprites happened to land, which
		// is why AUTO did nothing - the press was landing on a picture that
		// was not there. The panel draws chips instead and hit-tests them
		// itself, then calls the SAME button_pressed handlers, so none of the
		// spend logic is duplicated or reimplemented.
		static constexpr int16_t PANEL_CHIP_W = 26;
		static constexpr int16_t PANEL_CHIP_H = 15;

		// SPENDING IS ITS OWN ROW OF PROPER BUTTONS.
		//
		// It used to be a "+" chip beside each stat: 26 x 15, on a row pitch
		// of 15 - which is to say the buttons TOUCHED, top to bottom, six of
		// them in a column. Aiming at STR and hitting DEX twice was not bad
		// luck, it was the only likely outcome. A point spent is also not
		// undoable, which makes it the worst place in the game to put a
		// target smaller than a fingertip.
		//
		// Six named buttons across the bottom instead. 52 x 34 with a gap:
		// wider than a thumb, taller than a thumb, and each one says which
		// stat it is rather than relying on which line it happens to be on.
		static constexpr int16_t SPEND_W = 52;
		static constexpr int16_t SPEND_H = 34;
		static constexpr int16_t SPEND_GAP = 3;

		// How many are in the row, and in which order. HP and MP last: they
		// are the two nobody means to press.
		static constexpr size_t SPEND_COUNT = 6;

		// SPENDING LIVES IN A POPUP NOW.
		//
		// Six 52x34 buttons and an AUTO could not share a page with 24 rows
		// of statistics inside the panel's content box - something had to be
		// off the screen and it was always the buttons. A LEVEL UP button
		// costs one row and opens the rest over the top, which also matches
		// how it feels: spending points is a thing you do once, on purpose,
		// not a permanent fixture of looking at your character.
		bool spend_open = false;

		// NOTHING IS SPENT UNTIL IT IS LOCKED IN.
		//
		// Every chip press used to go straight down the wire as an AP-up, and
		// there is no undo for one of those - a mis-tap was permanent, on the
		// one screen in the game where that is true. The presses are counted
		// here instead and only sent when LOCK IN is pressed; GO BACK throws
		// the lot away and nothing ever left the device.
		//
		// Indexed the same as SPEND[] and panel_chip_box: STR DEX INT LUK HP MP.
		int16_t pending[SPEND_COUNT] = { 0, 0, 0, 0, 0, 0 };

		// How many points are spoken for, and how many are still free.
		int16_t pending_total() const;
		int16_t ap_left() const;

		// Send what was staged, then forget it. Called by LOCK IN.
		void commit_pending();

		// Forget it without sending. GO BACK, the X, and closing the page.
		void discard_pending();

		Rectangle<int16_t> panel_levelup_box() const;
		Rectangle<int16_t> panel_popup_box() const;
		Rectangle<int16_t> panel_popup_close() const;

		void draw_spend_popup() const;

		Rectangle<int16_t> panel_chip_box(size_t row) const;

		// The two that end it. Where AUTO used to be - see the note on its
		// removal in the .cpp.
		Rectangle<int16_t> panel_commit_box() const;
		Rectangle<int16_t> panel_cancel_box() const;

		// True when the press was ours.
		bool panel_pressed(Point<int16_t> at);

		mutable Text panel_chip_text;

		// Which rows appear in which column, in order.
		static const StatLabel PANEL_LEFT[];
		static const StatLabel PANEL_RIGHT[];
		static constexpr size_t PANEL_LEFT_COUNT = 11;
		static constexpr size_t PANEL_RIGHT_COUNT = 13;

		// The heading for a row. A switch, not an array: see the comment where
		// it is defined.
		static const char* heading_for(StatLabel label);

		// Where a row was drawn, so a button can be put beside it.
		int16_t panel_row_y(size_t row) const;

		// The row headings, which the artwork used to supply.
		Text panel_names[StatLabel::NUM_LABELS];

		void draw_panel_list() const;

		// What the sheet last showed for HP, MAXHP, MP, MAXMP and AP, so a
		// change can be noticed without waiting to be told about it.
		int32_t watched[5] = { -1, -1, -1, -1, -1 };

		void update_ap();
		void update_simple(StatLabel label, Maplestat::Id stat);
		void update_basevstotal(StatLabel label, Maplestat::Id bstat, Equipstat::Id tstat);
		void update_buffed(StatLabel label, Equipstat::Id stat);
		void send_apup(Maplestat::Id stat) const;
		void set_detail(bool enabled);

		enum Buttons
		{
			BT_CLOSE,
			BT_HP,
			BT_MP,
			BT_STR,
			BT_DEX,
			BT_INT,
			BT_LUK,
			BT_AUTO,
			BT_HYPERSTATOPEN,
			BT_HYPERSTATCLOSE,
			BT_DETAILOPEN,
			BT_DETAILCLOSE,
			BT_ABILITY,
			BT_DETAIL_DETAILCLOSE
		};

		const CharStats& stats;

		enum Ability
		{
			RARE,
			EPIC,
			UNIQUE,
			LEGENDARY,
			NONE,
			NUM_ABILITIES
		};

		std::array<Texture, UIStatsinfo::Ability::NUM_ABILITIES> abilities;
		BoolPair<Texture> inner_ability;

		std::vector<Texture> textures_detail;
		bool showdetail;

		bool hasap;

		Text statlabels[UIStatsinfo::StatLabel::NUM_LABELS];
		Point<int16_t> statoffsets[UIStatsinfo::StatLabel::NUM_LABELS];
	};
}