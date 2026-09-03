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
#include "UIStatsinfo.h"

#include "../UI.h"
#include "../../Graphics/GraphicsGL.h"

#include "../Components/MapleButton.h"
#include "../UITypes/UINotice.h"
#include "../Character/Player.h"
#include "../Gameplay/Stage.h"

#include "../Net/Packets/PlayerPackets.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UIStatsinfo::UIStatsinfo(const CharStats& st) : UIDragElement<PosSTATS>(Point<int16_t>(212, 20)), stats(st)
	{
		nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];
		nl::node Stat = nl::nx::ui["UIWindow4.img"]["Stat"];
		nl::node main = Stat["main"];
		nl::node detail = Stat["detail"];
		nl::node abilityTitle = detail["abilityTitle"];
		nl::node metierLine = detail["metierLine"];

		sprites.emplace_back(main["backgrnd"]);
		sprites.emplace_back(main["backgrnd2"]);
		sprites.emplace_back(main["backgrnd3"]);

		textures_detail.emplace_back(detail["backgrnd"]);
		textures_detail.emplace_back(detail["backgrnd2"]);
		textures_detail.emplace_back(detail["backgrnd3"]);
		textures_detail.emplace_back(detail["backgrnd4"]);

		abilities[Ability::RARE] = abilityTitle["rare"]["0"];
		abilities[Ability::EPIC] = abilityTitle["epic"]["0"];
		abilities[Ability::UNIQUE] = abilityTitle["unique"]["0"];
		abilities[Ability::LEGENDARY] = abilityTitle["legendary"]["0"];
		abilities[Ability::NONE] = abilityTitle["normal"]["0"];

		inner_ability[true] = metierLine["activated"]["0"];
		inner_ability[false] = metierLine["disabled"]["0"];

		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(close, Point<int16_t>(190, 6));
		buttons[Buttons::BT_HP] = std::make_unique<MapleButton>(main["BtHpUp"]);
		buttons[Buttons::BT_MP] = std::make_unique<MapleButton>(main["BtHpUp"], Point<int16_t>(0, 18));		// TODO: "BtMpUp" not Working
		buttons[Buttons::BT_STR] = std::make_unique<MapleButton>(main["BtHpUp"], Point<int16_t>(0, 87));	// TODO: "BtStrUp" not working
		buttons[Buttons::BT_DEX] = std::make_unique<MapleButton>(main["BtHpUp"], Point<int16_t>(0, 105));	// TODO: "BtDexUp" not working
		buttons[Buttons::BT_INT] = std::make_unique<MapleButton>(main["BtHpUp"], Point<int16_t>(0, 123));	// TODO: "BtIntUp" not working
		buttons[Buttons::BT_LUK] = std::make_unique<MapleButton>(main["BtHpUp"], Point<int16_t>(0, 141));	// TODO: "BtLukUp" not working
		// BT_AUTO IS DELIBERATELY NOT BUILT. Auto-assign was removed - see
		// the note above panel_cancel_box. The enumerator stays so the
		// button ids either side of it keep their values.
		buttons[Buttons::BT_HYPERSTATOPEN] = std::make_unique<MapleButton>(main["BtHyperStatOpen"]);
		buttons[Buttons::BT_HYPERSTATCLOSE] = std::make_unique<MapleButton>(main["BtHyperStatClose"]);
		buttons[Buttons::BT_DETAILOPEN] = std::make_unique<MapleButton>(main["BtDetailOpen"]);
		buttons[Buttons::BT_DETAILCLOSE] = std::make_unique<MapleButton>(main["BtDetailClose"]);
		buttons[Buttons::BT_ABILITY] = std::make_unique<MapleButton>(detail["BtAbility"], Point<int16_t>(212, 0));
		buttons[Buttons::BT_DETAIL_DETAILCLOSE] = std::make_unique<MapleButton>(detail["BtHpUp"], Point<int16_t>(212, 0));

		buttons[Buttons::BT_HYPERSTATOPEN]->set_active(false);
		buttons[Buttons::BT_DETAILCLOSE]->set_active(false);
		buttons[Buttons::BT_ABILITY]->set_active(false);
		buttons[Buttons::BT_ABILITY]->set_state(Button::State::DISABLED);
		buttons[Buttons::BT_DETAIL_DETAILCLOSE]->set_active(false);

		update_ap();

		// Normal
		for (size_t i = StatLabel::NAME; i <= LUK; i++)
			statlabels[i] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);

		statlabels[StatLabel::AP] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);

		statoffsets[StatLabel::NAME] = Point<int16_t>(73, 26);
		statoffsets[StatLabel::JOB] = Point<int16_t>(74, 44);
		statoffsets[StatLabel::GUILD] = Point<int16_t>(74, 63);
		statoffsets[StatLabel::FAME] = Point<int16_t>(74, 80);
		statoffsets[StatLabel::DAMAGE] = Point<int16_t>(74, 98);
		statoffsets[StatLabel::HP] = Point<int16_t>(74, 116);
		statoffsets[StatLabel::MP] = Point<int16_t>(74, 134);
		statoffsets[StatLabel::AP] = Point<int16_t>(91, 175);
		statoffsets[StatLabel::STR] = Point<int16_t>(73, 204);
		statoffsets[StatLabel::DEX] = Point<int16_t>(73, 222);
		statoffsets[StatLabel::INT] = Point<int16_t>(73, 240);
		statoffsets[StatLabel::LUK] = Point<int16_t>(73, 258);

		// Detailed
		statlabels[StatLabel::DAMAGE_DETAILED] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
		statlabels[StatLabel::DAMAGE_BONUS] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);
		statlabels[StatLabel::BOSS_DAMAGE] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);
		statlabels[StatLabel::FINAL_DAMAGE] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);
		statlabels[StatLabel::IGNORE_DEFENSE] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);
		statlabels[StatLabel::CRITICAL_RATE] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);
		statlabels[StatLabel::CRITICAL_DAMAGE] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
		statlabels[StatLabel::STATUS_RESISTANCE] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);
		statlabels[StatLabel::KNOCKBACK_RESISTANCE] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);
		statlabels[StatLabel::DEFENSE] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
		statlabels[StatLabel::SPEED] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);
		statlabels[StatLabel::JUMP] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);
		statlabels[StatLabel::HONOR] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);

		statoffsets[StatLabel::DAMAGE_DETAILED] = Point<int16_t>(73, 38);
		statoffsets[StatLabel::DAMAGE_BONUS] = Point<int16_t>(100, 56);
		statoffsets[StatLabel::BOSS_DAMAGE] = Point<int16_t>(196, 56);
		statoffsets[StatLabel::FINAL_DAMAGE] = Point<int16_t>(100, 74);
		statoffsets[StatLabel::IGNORE_DEFENSE] = Point<int16_t>(196, 74);
		statoffsets[StatLabel::CRITICAL_RATE] = Point<int16_t>(100, 92);
		statoffsets[StatLabel::CRITICAL_DAMAGE] = Point<int16_t>(73, 110);
		statoffsets[StatLabel::STATUS_RESISTANCE] = Point<int16_t>(100, 128);
		statoffsets[StatLabel::KNOCKBACK_RESISTANCE] = Point<int16_t>(196, 128);
		statoffsets[StatLabel::DEFENSE] = Point<int16_t>(73, 146);
		statoffsets[StatLabel::SPEED] = Point<int16_t>(100, 164);
		statoffsets[StatLabel::JUMP] = Point<int16_t>(196, 164);
		statoffsets[StatLabel::HONOR] = Point<int16_t>(73, 283);

		update_all_stats();
		update_stat(Maplestat::Id::JOB);
		update_stat(Maplestat::Id::FAME);

		dimension = Point<int16_t>(212, 318);
		showdetail = false;
	}

	// Who the character is, and what can be spent. The spend arrows sit
	// beside the four attributes and HP/MP.
	const UIStatsinfo::StatLabel UIStatsinfo::PANEL_LEFT[] = {
		StatLabel::NAME, StatLabel::JOB, StatLabel::GUILD, StatLabel::FAME,
		StatLabel::AP, StatLabel::HP, StatLabel::MP,
		StatLabel::STR, StatLabel::DEX, StatLabel::INT, StatLabel::LUK
	};

	// Everything worked out from the above.
	const UIStatsinfo::StatLabel UIStatsinfo::PANEL_RIGHT[] = {
		StatLabel::DAMAGE, StatLabel::DAMAGE_BONUS, StatLabel::BOSS_DAMAGE,
		StatLabel::FINAL_DAMAGE, StatLabel::IGNORE_DEFENSE,
		StatLabel::CRITICAL_RATE, StatLabel::CRITICAL_DAMAGE,
		StatLabel::STATUS_RESISTANCE, StatLabel::KNOCKBACK_RESISTANCE,
		StatLabel::DEFENSE, StatLabel::SPEED, StatLabel::JUMP, StatLabel::HONOR
	};

	const char* UIStatsinfo::heading_for(StatLabel label)
	{
		switch (label)
		{
		case StatLabel::NAME:                 return "NAME";
		case StatLabel::JOB:                  return "CLASS";
		case StatLabel::GUILD:                return "GUILD";
		case StatLabel::FAME:                 return "FAME";
		case StatLabel::DAMAGE:               return "DAMAGE";
		case StatLabel::HP:                   return "HP";
		case StatLabel::MP:                   return "MP";
		case StatLabel::AP:                   return "AP";
		case StatLabel::STR:                  return "STR";
		case StatLabel::DEX:                  return "DEX";
		case StatLabel::INT:                  return "INT";
		case StatLabel::LUK:                  return "LUK";
		case StatLabel::DAMAGE_BONUS:         return "DMG BONUS";
		case StatLabel::BOSS_DAMAGE:          return "BOSS DMG";
		case StatLabel::FINAL_DAMAGE:         return "FINAL DMG";
		case StatLabel::IGNORE_DEFENSE:       return "IGNORE DEF";
		case StatLabel::CRITICAL_RATE:        return "CRIT RATE";
		case StatLabel::CRITICAL_DAMAGE:      return "CRIT DMG";
		case StatLabel::STATUS_RESISTANCE:    return "STATUS RES";
		case StatLabel::KNOCKBACK_RESISTANCE: return "KNOCKBACK";
		case StatLabel::DEFENSE:              return "DEFENSE";
		case StatLabel::SPEED:                return "SPEED";
		case StatLabel::JUMP:                 return "JUMP";
		case StatLabel::HONOR:                return "HONOUR";
		default:                              return "";
		}
	}

	int16_t UIStatsinfo::panel_row_y(size_t row) const
	{
		return PANEL_TOP + static_cast<int16_t>(row) * PANEL_ROW_H;
	}

	Rectangle<int16_t> UIStatsinfo::panel_popup_box() const
	{
		// Most of the page, so the buttons inside it can be thumb-sized. It
		// is drawn over the statistics, which nobody is reading at the moment
		// they are spending a point.
		return Rectangle<int16_t>(
			Point<int16_t>(6, 24),
			Point<int16_t>(static_cast<int16_t>(panel_screen.x() - 6),
				static_cast<int16_t>(panel_screen.y() - 10)));
	}

	Rectangle<int16_t> UIStatsinfo::panel_popup_close() const
	{
		Rectangle<int16_t> box = panel_popup_box();

		return Rectangle<int16_t>(
			Point<int16_t>(static_cast<int16_t>(box.right() - 34), box.top() + 4),
			Point<int16_t>(static_cast<int16_t>(box.right() - 6),
				static_cast<int16_t>(box.top() + 28)));
	}

	Rectangle<int16_t> UIStatsinfo::panel_levelup_box() const
	{
		// MEASURED FROM THE BOTTOM OF THE BOX, not from the end of the rows.
		//
		// Placed after the last stat row it sat wherever thirteen rows
		// happened to end - which on this panel was below the content box, so
		// "NOTHING TO SPEND" was printed straight over the HP and MP numbers.
		// The bottom of the page is a fact; the height of the sheet is not.
		constexpr int16_t H = 28;

		int16_t y = static_cast<int16_t>(panel_screen.y() - H - 2);

		return Rectangle<int16_t>(
			Point<int16_t>(PANEL_LEFT_X, y),
			Point<int16_t>(static_cast<int16_t>(panel_screen.x() - PANEL_LEFT_X),
				static_cast<int16_t>(y + H)));
	}

	Rectangle<int16_t> UIStatsinfo::panel_chip_box(size_t index) const
	{
		// TWO ROWS OF THREE, INSIDE THE POPUP. A single row of six across a
		// 344-wide panel gives each button 52 pixels; two rows give them 90,
		// which is a thumb rather than a fingernail - and this is the one
		// screen in the game where a mis-tap cannot be undone.
		Rectangle<int16_t> box = panel_popup_box();

		constexpr int16_t COLS = 3;
		constexpr int16_t W = 92;
		constexpr int16_t H = 44;
		constexpr int16_t GAP = 8;

		int16_t total = static_cast<int16_t>(COLS * W + (COLS - 1) * GAP);
		int16_t left = static_cast<int16_t>(box.left() + (box.width() - total) / 2);

		int16_t col = static_cast<int16_t>(index % COLS);
		int16_t row = static_cast<int16_t>(index / COLS);

		int16_t x = static_cast<int16_t>(left + col * (W + GAP));
		int16_t y = static_cast<int16_t>(box.top() + 40 + row * (H + GAP));

		return Rectangle<int16_t>(
			Point<int16_t>(x, y),
			Point<int16_t>(static_cast<int16_t>(x + W),
				static_cast<int16_t>(y + H)));
	}

	// AUTO IS GONE, NOT DISABLED.
	//
	// It was one button that spent every point you had, in one press, with no
	// undo - and it could not know where they should go. For a Beginner it
	// guessed strength, which is the wrong answer for three of the four
	// classes; refusing for Beginners only narrowed the accident rather than
	// removing it, because it still poured the lot into one stat for everyone
	// else the moment it was pressed. There is no version of "spend all of it
	// for me" that is safe on a screen with no undo, so its room went to the
	// two buttons that make the rest of the popup safe instead.
	namespace
	{
		// The end-of-popup row: GO BACK on the left, LOCK IN on the right.
		constexpr int16_t END_H = 34;
		constexpr int16_t END_GAP = 10;
	}

	Rectangle<int16_t> UIStatsinfo::panel_cancel_box() const
	{
		Rectangle<int16_t> box = panel_popup_box();

		int16_t y = static_cast<int16_t>(box.bottom() - END_H - 8);
		int16_t half = static_cast<int16_t>((box.width() - 60 - END_GAP) / 2);

		return Rectangle<int16_t>(
			Point<int16_t>(static_cast<int16_t>(box.left() + 30), y),
			Point<int16_t>(static_cast<int16_t>(box.left() + 30 + half),
				static_cast<int16_t>(y + END_H)));
	}

	Rectangle<int16_t> UIStatsinfo::panel_commit_box() const
	{
		Rectangle<int16_t> box = panel_popup_box();
		Rectangle<int16_t> back = panel_cancel_box();

		return Rectangle<int16_t>(
			Point<int16_t>(static_cast<int16_t>(back.right() + END_GAP),
				back.top()),
			Point<int16_t>(static_cast<int16_t>(box.right() - 30),
				back.bottom()));
	}

	int16_t UIStatsinfo::pending_total() const
	{
		int16_t total = 0;

		for (size_t i = 0; i < SPEND_COUNT; i++)
			total = static_cast<int16_t>(total + pending[i]);

		return total;
	}

	int16_t UIStatsinfo::ap_left() const
	{
		int16_t have = static_cast<int16_t>(stats.get_stat(Maplestat::Id::AP));

		return static_cast<int16_t>(have - pending_total());
	}

	void UIStatsinfo::discard_pending()
	{
		for (size_t i = 0; i < SPEND_COUNT; i++)
			pending[i] = 0;
	}

	void UIStatsinfo::commit_pending()
	{
		static const Maplestat::Id STAT[SPEND_COUNT] = {
			Maplestat::Id::STR,
			Maplestat::Id::DEX,
			Maplestat::Id::INT,
			Maplestat::Id::LUK,
			Maplestat::Id::HP,
			Maplestat::Id::MP
		};

		for (size_t i = 0; i < SPEND_COUNT; i++)
			for (int16_t n = 0; n < pending[i]; n++)
				send_apup(STAT[i]);

		discard_pending();
	}

	bool UIStatsinfo::panel_pressed(Point<int16_t> at)
	{
		if (!hasap)
			return false;

		// NOTHING INSIDE THE POPUP IS LIVE WHILE THE POPUP IS SHUT.
		//
		// This is the bug that made LEVEL UP dump every point into STR. The
		// six chips and AUTO were tested on every press regardless of whether
		// the popup was open, and `panel_auto_box` - which lives along the
		// bottom of the popup - OVERLAPS the LEVEL UP bar by twelve pixels,
		// because the bar is measured from the bottom of the page and the
		// popup ends ten pixels above it.
		//
		// So a thumb on the gold bar landed on an invisible "AUTO - SPEND
		// THEM ALL", which for a Beginner is all of them into strength. The
		// popup that was supposed to open was never opened by anything at
		// all: nothing in this file ever set `spend_open` to true.
		if (!spend_open)
		{
			if (panel_levelup_box().contains(at))
			{
				spend_open = true;

				return true;
			}

			return false;
		}

		// Shut it again. Drawn top-right of the popup as an X.
		//
		// The X DISCARDS. Anything staged and not locked in was never sent, so
		// closing the popup has to throw it away rather than leave it waiting
		// to be committed by something else later.
		if (panel_popup_close().contains(at))
		{
			discard_pending();

			spend_open = false;

			return true;
		}

		if (panel_cancel_box().contains(at))
		{
			discard_pending();

			spend_open = false;

			return true;
		}

		if (panel_commit_box().contains(at))
		{
			// Nothing staged is not an error, just nothing to do - and it
			// still shuts, because that is what pressing it means.
			commit_pending();

			spend_open = false;

			return true;
		}

		// THE FOUR ATTRIBUTES FIRST, then HP and MP - the order the chips are
		// drawn in, and the order commit_pending sends them in. The window's
		// own Buttons are no longer involved: a chip stages a number now, it
		// does not press a button.
		for (size_t i = 0; i < SPEND_COUNT; i++)
		{
			if (panel_chip_box(i).contains(at))
			{
				// STAGED, NOT SENT. The wire is only touched by LOCK IN.
				//
				// Silently ignored once every point is spoken for, which the
				// button already shows by going dark - there is nothing to
				// explain and an error box for pressing a dead button is
				// worse than the button simply not moving.
				if (ap_left() > 0)
					pending[i] = static_cast<int16_t>(pending[i] + 1);

				return true;
			}
		}

		// A PRESS ANYWHERE ELSE ON THE POPUP STOPS HERE.
		//
		// The popup covers the statistics; letting a press fall through it
		// would work the page underneath, which the player cannot see.
		return panel_popup_box().contains(at);
	}

	Cursor::State UIStatsinfo::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// THE PANEL'S CHIPS FIRST.
		//
		// Below this is UIDragElement, which on the panel is only ever going
		// to try to drag a window that is pinned, and then UIElement's button
		// pass, whose buttons are all switched off here. Neither would ever
		// reach the spend controls, which is why AUTO did nothing.
		if (panel && clicked && panel_pressed(cursorpos - position))
			return Cursor::State::IDLE;

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIStatsinfo::set_panel(Point<int16_t> screen)
	{
		panel = true;
		panel_screen = screen;

		// Nothing to close and nothing to drag on a page.
		buttons[Buttons::BT_CLOSE]->set_active(false);

		// Everything at once. The Detail button is what this page exists
		// without - there is no reason to hide half a character sheet behind a
		// toggle on a screen that is always showing.
		showdetail = true;

		buttons[Buttons::BT_DETAILOPEN]->set_active(false);
		buttons[Buttons::BT_DETAILCLOSE]->set_active(false);
		buttons[Buttons::BT_DETAIL_DETAILCLOSE]->set_active(false);
		buttons[Buttons::BT_HYPERSTATOPEN]->set_active(false);
		buttons[Buttons::BT_HYPERSTATCLOSE]->set_active(false);
		buttons[Buttons::BT_ABILITY]->set_active(false);

		// The headings the artwork used to carry, now that none of it is drawn.
		//
		// Written as a switch rather than an array literal on purpose.
		// StatLabel puts NUM_NORMAL in the MIDDLE of its own enumerators, so
		// the values are not contiguous and NUM_LABELS is one larger than the
		// number of real labels. A positional list silently shifts from that
		// point on and leaves the last entry null - which is a crash inside
		// strlen the moment the page is opened, and exactly what happened.
		// Naming each one cannot drift.
		for (size_t i = 0; i < StatLabel::NUM_LABELS; i++)
			panel_names[i] = Text(Text::Font::A11M, Text::Alignment::LEFT,
				Color::Name::WHITE, heading_for(static_cast<StatLabel>(i)));

		// The four attributes and the two gauges take a point each.
		struct Spend { Buttons button; StatLabel row; };

		static const Spend SPEND[] = {
			{ Buttons::BT_HP,  StatLabel::HP  },
			{ Buttons::BT_MP,  StatLabel::MP  },
			{ Buttons::BT_STR, StatLabel::STR },
			{ Buttons::BT_DEX, StatLabel::DEX },
			{ Buttons::BT_INT, StatLabel::INT },
			{ Buttons::BT_LUK, StatLabel::LUK }
		};

		// A button draws at its position MINUS its artwork's origin, so to
		// land somewhere the origin is ADDED. Subtracting doubles the offset,
		// which is how these once ended up off the side of the screen.
		for (const Spend& s : SPEND)
		{
			// Find which row of the left column this stat is on.
			size_t row = 0;

			for (size_t i = 0; i < PANEL_LEFT_COUNT; i++)
				if (PANEL_LEFT[i] == s.row)
					row = i;

			Point<int16_t> at = Point<int16_t>(
				PANEL_LEFT_X + PANEL_ARROW_X, panel_row_y(row) - 3);

			buttons[s.button]->set_position(at + buttons[s.button]->origin());

			// Always there, whether or not anything can be spent. A button
			// that comes and goes is one the player has to hunt for, and its
			// disabled state already says it cannot be used.
			buttons[s.button]->set_active(true);
		}

		// AND THEN EVERY ONE OF THEM OFF AGAIN.
		//
		// Positioned above so the non-panel window is unaffected, then
		// deactivated here because on the panel the chips below do the job.
		// A MapleButton with no artwork under it is an invisible hit box in
		// the middle of a page of stats.
		for (const Spend& s : SPEND)
			buttons[s.button]->set_active(false);

		// Pale values, not the near-black the window used - they sit on a dark
		// plate here rather than on white artwork.
		for (size_t i = 0; i < StatLabel::NUM_LABELS; i++)
			statlabels[i] = Text(Text::Font::A11M, Text::Alignment::LEFT,
				Color::Name::WHITE);

		update_all_stats();
		update_stat(Maplestat::Id::JOB);
		update_stat(Maplestat::Id::FAME);

		// The whole panel, so the page centres it at x 0 and both columns land
		// where they were measured from.
		dimension = Point<int16_t>(screen.x(),
			panel_row_y(PANEL_LEFT_COUNT) + 30);
	}

	void UIStatsinfo::update()
	{
		UIElement::update();

		// Watched rather than waited for.
		//
		// Losing HP arrives as a stat change that asks the whole sheet to be
		// recalculated, and the recalculation is what refreshes these - so a
		// number could sit stale while the bar above it had already moved.
		// Comparing what is drawn against what the character actually has
		// costs nothing and cannot fall out of step.
		int32_t now[] = {
			stats.get_stat(Maplestat::Id::HP),
			stats.get_stat(Maplestat::Id::MAXHP),
			stats.get_stat(Maplestat::Id::MP),
			stats.get_stat(Maplestat::Id::MAXMP),
			stats.get_stat(Maplestat::Id::AP)
		};

		Maplestat::Id which[] = {
			Maplestat::Id::HP, Maplestat::Id::MAXHP,
			Maplestat::Id::MP, Maplestat::Id::MAXMP,
			Maplestat::Id::AP
		};

		for (int i = 0; i < 5; i++)
		{
			if (now[i] == watched[i])
				continue;

			watched[i] = now[i];

			update_stat(which[i]);
		}
	}

	void UIStatsinfo::draw_spend_popup() const
	{
		Rectangle<int16_t> box = panel_popup_box();

		// A PLATE THICK ENOUGH TO BE A DIFFERENT SURFACE. The statistics
		// behind it stop competing for attention - this is a decision, and
		// the page underneath is reference material.
		GraphicsGL::get().drawrectangle(
			position.x() + box.left(), position.y() + box.top(),
			box.width(), box.height(), 0.06f, 0.07f, 0.10f, 0.97f);

		int16_t ap = ap_left();

		// WHAT IS STILL FREE, not what the character owns. Points already
		// staged are spoken for even though nothing has been sent, and a
		// header that kept counting them as available would invite a player
		// to allocate more than they have and then wonder which ones took.
		panel_chip_text.change_text(
			"SPEND A POINT - " + std::to_string(ap) + " LEFT");
		panel_chip_text.draw(Point<int16_t>(
			position.x() + box.left() + box.width() / 2,
			position.y() + box.top() + 10));

		Rectangle<int16_t> shut = panel_popup_close();

		GraphicsGL::get().drawrectangle(
			position.x() + shut.left(), position.y() + shut.top(),
			shut.width(), shut.height(), 0.45f, 0.16f, 0.16f, 0.92f);

		panel_chip_text.change_text("X");
		panel_chip_text.draw(Point<int16_t>(
			position.x() + shut.left() + shut.width() / 2,
			position.y() + shut.top() + 4));

		static const char* SPEND_NAME[] = {
			"STR", "DEX", "INT", "LUK", "HP", "MP"
		};

		// WHAT THE STAT IS NOW, under its own button.
		//
		// Spending a point is a decision, and the number it is being added to
		// is the whole of the information needed to make it. Without this the
		// popup covers the sheet with the figures on it, so deciding meant
		// closing the popup, reading, and opening it again.
		//
		// HP and MP show the MAXIMUM, because that is what the point raises -
		// the current value is a matter of how recently you were hit.
		auto current = [&](size_t index) -> int32_t
		{
			switch (index)
			{
			case 0: return stats.get_stat(Maplestat::Id::STR);
			case 1: return stats.get_stat(Maplestat::Id::DEX);
			case 2: return stats.get_stat(Maplestat::Id::INT);
			case 3: return stats.get_stat(Maplestat::Id::LUK);
			case 4: return stats.get_stat(Maplestat::Id::MAXHP);
			default: return stats.get_stat(Maplestat::Id::MAXMP);
			}
		};

		auto button = [&](Rectangle<int16_t> at, const std::string& label, bool live,
			const std::string& below)
		{
			GraphicsGL::get().drawrectangle(
				position.x() + at.left(), position.y() + at.top(),
				at.width(), at.height(),
				live ? 0.86f : 0.18f, live ? 0.74f : 0.18f,
				live ? 0.36f : 0.20f, live ? 0.92f : 0.45f);

			// The label sits higher when there is a number under it, so the
			// pair reads as one block rather than as text that slipped.
			int16_t lift = below.empty() ? 8 : 15;

			panel_chip_text.change_text(label);
			panel_chip_text.draw(Point<int16_t>(
				position.x() + at.left() + at.width() / 2,
				position.y() + at.top() + at.height() / 2 - lift));

			if (!below.empty())
			{
				panel_chip_text.change_text(below);
				panel_chip_text.draw(Point<int16_t>(
					position.x() + at.left() + at.width() / 2,
					position.y() + at.top() + at.height() / 2 + 1));
			}
		};

		// WHAT IT IS NOW, AND WHAT IT WOULD BE. A staged point shows as
		// "4 -> 5" under its button, so the whole plan can be read off the
		// popup before any of it is sent - which is the point of staging it.
		//
		// HP and MP are shown without a target: a point there adds an amount
		// the server decides from the class, and inventing a number here would
		// be a guess printed as a fact.
		for (size_t i = 0; i < SPEND_COUNT; i++)
		{
			std::string below = std::to_string(current(i));

			if (pending[i] > 0)
				below += (i < 4)
					? " > " + std::to_string(current(i) + pending[i])
					: " +" + std::to_string(pending[i]);

			button(panel_chip_box(i), SPEND_NAME[i], ap > 0, below);
		}

		// GO BACK is always live - there is always something to go back from.
		// LOCK IN only lights up once something has been staged, so a player
		// who opened the popup by accident is not offered a confirmation for
		// a decision they have not made.
		button(panel_cancel_box(), "GO BACK", true, "");
		button(panel_commit_box(),
			pending_total() > 0
				? "LOCK IN " + std::to_string(pending_total())
				: std::string("LOCK IN"),
			pending_total() > 0, "");
	}

	void UIStatsinfo::draw_panel_list() const
	{
		// NO PLATE. It was black at 0.45 over the whole sheet, which put a
		// dark rectangle in the middle of a panel whose other pages show the
		// frame through. The hairline below is enough to say there are two
		// columns; the text is outlined and reads on the frame directly.

		// A hairline between the columns, so the eye knows there are two.
		GraphicsGL::get().drawrectangle(
			position.x() + PANEL_RIGHT_X - 6, position.y() + PANEL_TOP,
			1, PANEL_RIGHT_COUNT * PANEL_ROW_H,
			1.0f, 1.0f, 1.0f, 0.18f);

		for (size_t i = 0; i < PANEL_LEFT_COUNT; i++)
		{
			StatLabel row = PANEL_LEFT[i];
			int16_t y = position.y() + panel_row_y(i);

			panel_names[row].draw(Point<int16_t>(position.x() + PANEL_LEFT_X, y));
			statlabels[row].draw(Point<int16_t>(position.x() + PANEL_LEFT_X + PANEL_VALUE_X, y));
		}

		for (size_t i = 0; i < PANEL_RIGHT_COUNT; i++)
		{
			StatLabel row = PANEL_RIGHT[i];
			int16_t y = position.y() + panel_row_y(i);

			panel_names[row].draw(Point<int16_t>(position.x() + PANEL_RIGHT_X, y));
			statlabels[row].draw(Point<int16_t>(position.x() + PANEL_RIGHT_X + PANEL_VALUE_X + 20, y));
		}

		// THE SPEND CHIPS. Only while there is anything to spend - unlike the
		// window's arrows, which are always there and merely greyed, because
		// on a page this dense six dead boxes are six things to read past.
		if (panel_chip_text.get_text().empty())
			panel_chip_text = Text(Text::Font::A11B, Text::Alignment::CENTER,
				Color::Name::WHITE);

		// THE LEVEL UP BUTTON.
		//
		// Always drawn, and GREYED when there is nothing to spend, rather than
		// appearing and disappearing. A control that comes and goes is one the
		// player has to hunt for; a dull one still says where it will be when
		// it matters.
		Rectangle<int16_t> up = panel_levelup_box();

		GraphicsGL::get().drawrectangle(
			position.x() + up.left(), position.y() + up.top(),
			up.width(), up.height(),
			hasap ? 0.86f : 0.18f, hasap ? 0.74f : 0.18f,
			hasap ? 0.36f : 0.20f, hasap ? 0.92f : 0.45f);

		int16_t ap = stats.get_stat(Maplestat::Id::AP);

		panel_chip_text.change_text(hasap
			? ("LEVEL UP - " + std::to_string(ap) + " POINTS")
			: "NOTHING TO SPEND");

		panel_chip_text.draw(Point<int16_t>(
			position.x() + up.left() + up.width() / 2,
			position.y() + up.top() + 6));

		if (spend_open)
			draw_spend_popup();
	}

	bool UIStatsinfo::indragrange(Point<int16_t> cursorpos) const
	{
		if (panel)
			return false;

		return UIDragElement::indragrange(cursorpos);
	}

	void UIStatsinfo::draw(float alpha) const
	{
		// Faint on the panel, where there is a picture behind it. Only the
		// window's own plate fades - every label and number on top of it is
		// drawn exactly as before.
		if (panel)
			draw_panel_list();
		else
			UIElement::draw_sprites(alpha);

		// The panel's sheet is drawn whole by draw_panel_list, including every
		// value - none of the artwork below belongs to it.
		if (showdetail && !panel)
		{
			Point<int16_t> detail_pos(position + Point<int16_t>(212, 0));

			// The same treatment as the window it opens beside - otherwise the
			// detail column arrives as a solid white slab next to a faint one.
			float fade = panel ? PANEL_FADE : 1.0f;

			textures_detail[0].draw(DrawArgument(detail_pos + Point<int16_t>(0, -1), fade));
			textures_detail[1].draw(DrawArgument(detail_pos, fade));
			textures_detail[2].draw(DrawArgument(detail_pos, fade));
			textures_detail[3].draw(DrawArgument(detail_pos, fade));

			abilities[Ability::NONE].draw(DrawArgument(detail_pos, fade));

			inner_ability[false].draw(DrawArgument(detail_pos, fade));
			inner_ability[false].draw(DrawArgument(detail_pos + Point<int16_t>(0, 19), fade));
			inner_ability[false].draw(DrawArgument(detail_pos + Point<int16_t>(0, 38), fade));
		}

		if (!panel)
		{
			size_t last = showdetail ? StatLabel::NUM_LABELS : StatLabel::NUM_NORMAL;

			for (size_t i = 0; i < last; i++)
			{
				Point<int16_t> labelpos = position + statoffsets[i];

				if (i >= StatLabel::NUM_NORMAL)
					labelpos.shift_x(213);

				statlabels[i].draw(labelpos);
			}
		}

		UIElement::draw_buttons(alpha);
	}

	void UIStatsinfo::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	bool UIStatsinfo::is_in_range(Point<int16_t> cursorpos) const
	{
		Point<int16_t> pos_adj;

		if (showdetail)
			pos_adj = Point<int16_t>(211, 25);
		else
			pos_adj = Point<int16_t>(0, 0);

		auto bounds = Rectangle<int16_t>(position, position + dimension + pos_adj);
		return bounds.contains(cursorpos);
	}

	UIElement::Type UIStatsinfo::get_type() const
	{
		return TYPE;
	}

	void UIStatsinfo::update_all_stats()
	{
		update_simple(AP, Maplestat::Id::AP);

		if (hasap ^ (stats.get_stat(Maplestat::Id::AP) > 0))
			update_ap();

		statlabels[StatLabel::NAME].change_text(stats.get_name());
		statlabels[StatLabel::GUILD].change_text("-");
		statlabels[StatLabel::HP].change_text(std::to_string(stats.get_stat(Maplestat::Id::HP)) + " / " + std::to_string(stats.get_total(Equipstat::Id::HP)));
		statlabels[StatLabel::MP].change_text(std::to_string(stats.get_stat(Maplestat::Id::MP)) + " / " + std::to_string(stats.get_total(Equipstat::Id::MP)));

		update_basevstotal(StatLabel::STR, Maplestat::Id::STR, Equipstat::Id::STR);
		update_basevstotal(StatLabel::DEX, Maplestat::Id::DEX, Equipstat::Id::DEX);
		update_basevstotal(StatLabel::INT, Maplestat::Id::INT, Equipstat::Id::INT);
		update_basevstotal(StatLabel::LUK, Maplestat::Id::LUK, Equipstat::Id::LUK);

		statlabels[StatLabel::DAMAGE].change_text(std::to_string(stats.get_mindamage()) + " ~ " + std::to_string(stats.get_maxdamage()));

		if (stats.is_damage_buffed())
			statlabels[StatLabel::DAMAGE].change_color(Color::Name::RED);
		else
			statlabels[StatLabel::DAMAGE].change_color(Color::Name::EMPEROR);

		statlabels[StatLabel::DAMAGE_DETAILED].change_text(std::to_string(stats.get_mindamage()) + " ~ " + std::to_string(stats.get_maxdamage()));
		statlabels[StatLabel::DAMAGE_BONUS].change_text("0%");
		statlabels[StatLabel::BOSS_DAMAGE].change_text(std::to_string(static_cast<int32_t>(stats.get_bossdmg() * 100)) + "%");
		statlabels[StatLabel::FINAL_DAMAGE].change_text("0%");
		statlabels[StatLabel::IGNORE_DEFENSE].change_text(std::to_string(static_cast<int32_t>(stats.get_ignoredef())) + "%");
		statlabels[StatLabel::CRITICAL_RATE].change_text(std::to_string(static_cast<int32_t>(stats.get_critical() * 100)) + "%");
		statlabels[StatLabel::CRITICAL_DAMAGE].change_text("0.00%");
		statlabels[StatLabel::STATUS_RESISTANCE].change_text(std::to_string(static_cast<int32_t>(stats.get_resistance())));
		statlabels[StatLabel::KNOCKBACK_RESISTANCE].change_text("0%");

		update_buffed(StatLabel::DEFENSE, Equipstat::Id::WDEF);

		statlabels[StatLabel::SPEED].change_text(std::to_string(stats.get_total(Equipstat::Id::SPEED)) + "%");
		statlabels[StatLabel::JUMP].change_text(std::to_string(stats.get_total(Equipstat::Id::JUMP)) + "%");
		statlabels[StatLabel::HONOR].change_text(std::to_string(stats.get_honor()));
	}

	void UIStatsinfo::update_stat(Maplestat::Id stat)
	{
		switch (stat)
		{
		case Maplestat::Id::JOB:
			statlabels[StatLabel::JOB].change_text(stats.get_jobname());
			break;
		case Maplestat::Id::FAME:
			update_simple(StatLabel::FAME, Maplestat::Id::FAME);
			break;
		}
	}

	Button::State UIStatsinfo::button_pressed(uint16_t id)
	{
		const Player& player = Stage::get().get_player();

		switch (id)
		{
		case Buttons::BT_CLOSE:
			deactivate();
			break;
		case Buttons::BT_HP:
			send_apup(Maplestat::Id::HP);
			break;
		case Buttons::BT_MP:
			send_apup(Maplestat::Id::MP);
			break;
		case Buttons::BT_STR:
			send_apup(Maplestat::Id::STR);
			break;
		case Buttons::BT_DEX:
			send_apup(Maplestat::Id::DEX);
			break;
		case Buttons::BT_INT:
			send_apup(Maplestat::Id::INT);
			break;
		case Buttons::BT_LUK:
			send_apup(Maplestat::Id::LUK);
			break;
		// BT_AUTO REMOVED. Auto-assign is gone entirely - see the note
		// above panel_cancel_box. No button raises this id any more.
		case Buttons::BT_HYPERSTATOPEN:
			break;
		case Buttons::BT_HYPERSTATCLOSE:
		{
			if (player.get_level() < 140)
				UI::get().emplace<UIOk>("You can use the Hyper Stat at Lv. 140 and above.", [](bool) {});
		}
		break;
		case Buttons::BT_DETAILOPEN:
			set_detail(true);
			break;
		case Buttons::BT_DETAILCLOSE:
		case Buttons::BT_DETAIL_DETAILCLOSE:
			set_detail(false);
			break;
		case Buttons::BT_ABILITY:
			break;
		default:
			break;
		}

		return Button::State::NORMAL;
	}

	void UIStatsinfo::send_apup(Maplestat::Id stat) const
	{
		SpendApPacket(stat).dispatch();
		UI::get().disable();
	}

	void UIStatsinfo::set_detail(bool enabled)
	{
		showdetail = enabled;

		buttons[Buttons::BT_DETAILOPEN]->set_active(!enabled);
		buttons[Buttons::BT_DETAILCLOSE]->set_active(enabled);
		buttons[Buttons::BT_ABILITY]->set_active(enabled);
		buttons[Buttons::BT_DETAIL_DETAILCLOSE]->set_active(enabled);
	}

	void UIStatsinfo::update_ap()
	{
		bool nowap = stats.get_stat(Maplestat::Id::AP) > 0;
		Button::State newstate = nowap ? Button::State::NORMAL : Button::State::DISABLED;

		// SPENT THE LAST ONE - PUT THE POPUP AWAY.
		//
		// Otherwise it stays open over the sheet with six dead buttons, and
		// the one thing the player wants to see at that moment is what their
		// stats have just become.
		//
		// Nothing staged can survive this. The AP total moving is either the
		// commit landing, in which case the staging is already cleared, or a
		// level-up handing out more - and a plan drawn against the old total
		// must not be carried across into the new one.
		if (!nowap)
		{
			discard_pending();

			spend_open = false;
		}

		// STOPS SHORT OF BT_AUTO, which is no longer built. The loop used to
		// include it, and reaching past the last live button would dereference
		// a null unique_ptr on every AP change - which is every level-up.
		for (int i = Buttons::BT_HP; i < Buttons::BT_AUTO; i++)
			buttons[i]->set_state(newstate);

		hasap = nowap;
	}

	void UIStatsinfo::update_simple(StatLabel label, Maplestat::Id stat)
	{
		statlabels[label].change_text(std::to_string(stats.get_stat(stat)));
	}

	void UIStatsinfo::update_basevstotal(StatLabel label, Maplestat::Id bstat, Equipstat::Id tstat)
	{
		int32_t base = stats.get_stat(bstat);
		int32_t total = stats.get_total(tstat);
		int32_t delta = total - base;

		std::string stattext = std::to_string(total);

		if (delta)
		{
			stattext += " (" + std::to_string(base);

			if (delta > 0)
				stattext += "+" + std::to_string(delta);
			else if (delta < 0)
				stattext += "-" + std::to_string(-delta);

			stattext += ")";
		}

		statlabels[label].change_text(stattext);
	}

	void UIStatsinfo::update_buffed(StatLabel label, Equipstat::Id stat)
	{
		int32_t total = stats.get_total(stat);
		int32_t delta = stats.get_buffdelta(stat);

		std::string stattext = std::to_string(total);

		if (delta)
		{
			stattext += " (" + std::to_string(total - delta);

			if (delta > 0)
			{
				stattext += "+" + std::to_string(delta);

				statlabels[label].change_color(Color::Name::RED);
			}
			else if (delta < 0)
			{
				stattext += "-" + std::to_string(-delta);

				statlabels[label].change_color(Color::Name::BLUE);
			}

			stattext += ")";
		}

		statlabels[label].change_text(stattext);
	}
}