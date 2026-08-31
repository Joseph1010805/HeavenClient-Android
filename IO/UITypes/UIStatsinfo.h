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
		static constexpr int16_t PANEL_ROW_H = 15;
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
		static constexpr int16_t PANEL_AUTO_W = 74;

		// Right after the VALUE, not out at PANEL_ARROW_X - that is 148, and
		// the right-hand column starts at 176, so a 26-wide chip there ran
		// into "CRIT RATE".
		static constexpr int16_t PANEL_CHIP_X = 104;

		Rectangle<int16_t> panel_chip_box(size_t row) const;
		Rectangle<int16_t> panel_auto_box() const;

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