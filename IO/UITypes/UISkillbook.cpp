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
#include "UISkillbook.h"

#include "../Components/Icon.h"
#include "../Components/MapleButton.h"
#include "../Components/StatefulIcon.h"
#include "../Character/SkillId.h"
#include "../Data/JobData.h"
#include "../Data/SkillData.h"
#include "../Gameplay/Stage.h"
#include "../Keyboard.h"
#include "../KeyType.h"
#include "../IO/UI.h"

#include "../IO/UITypes/UIKeyConfig.h"
#include "../Net/Packets/PlayerPackets.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UISkillbook::SkillIcon::SkillIcon(int32_t id) : skill_id(id) {}

	void UISkillbook::SkillIcon::drop_on_bindings(Point<int16_t> cursorposition, bool remove) const
	{
		auto keyconfig = UI::get().get_element<UIKeyConfig>();
		Keyboard::Mapping mapping = Keyboard::Mapping(KeyType::SKILL, skill_id);

		if (remove)
			keyconfig->unstage_mapping(mapping);
		else
			keyconfig->stage_mapping(cursorposition, mapping);
	}

	Icon::IconType UISkillbook::SkillIcon::get_type()
	{
		return Icon::IconType::SKILL;
	}

	UISkillbook::SkillDisplayMeta::SkillDisplayMeta(int32_t i, int32_t l) : id(i), level(l)
	{
		const SkillData& data = SkillData::get(id);

		Texture ntx = data.get_icon(SkillData::Icon::NORMAL);
		Texture dtx = data.get_icon(SkillData::Icon::DISABLED);
		Texture motx = data.get_icon(SkillData::Icon::MOUSEOVER);
		icon = std::make_unique<StatefulIcon>(std::make_unique<SkillIcon>(id), ntx, dtx, motx);

		std::string namestr = data.get_name();
		std::string levelstr = std::to_string(level);

		name_text = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR, namestr);
		level_text = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR, levelstr);

		constexpr uint16_t MAX_NAME_WIDTH = 97;
		size_t overhang = 3;

		while (name_text.width() > MAX_NAME_WIDTH)
		{
			namestr.replace(namestr.end() - overhang, namestr.end(), "..");
			overhang += 1;

			name_text.change_text(namestr);
		}
	}

	void UISkillbook::SkillDisplayMeta::draw(const DrawArgument& args) const
	{
		icon->draw(args.getpos());
		name_text.draw(args + Point<int16_t>(38, -5));
		level_text.draw(args + Point<int16_t>(38, 13));
	}

	int32_t UISkillbook::SkillDisplayMeta::get_id() const
	{
		return id;
	}

	int32_t UISkillbook::SkillDisplayMeta::get_level() const
	{
		return level;
	}

	StatefulIcon* UISkillbook::SkillDisplayMeta::get_icon() const
	{
		return icon.get();
	}

	UISkillbook::UISkillbook(const CharStats& in_stats, const Skillbook& in_skillbook) : UIDragElement<PosSKILL>(), stats(in_stats), skillbook(in_skillbook), grabbing(false), tab(0), macro_enabled(false), sp_enabled(false)
	{
		nl::node Skill = nl::nx::ui["UIWindow2.img"]["Skill"];
		nl::node main = Skill["main"];
		nl::node ui_backgrnd = main["backgrnd"];

		bg_dimensions = Texture(ui_backgrnd).get_dimensions();

		skilld = main["skill0"];
		skille = main["skill1"];
		skillb = main["skillBlank"];
		line = main["line"];

		buttons[Buttons::BT_HYPER] = std::make_unique<MapleButton>(main["BtHyper"]);
		buttons[Buttons::BT_GUILDSKILL] = std::make_unique<MapleButton>(main["BtGuildSkill"]);
		buttons[Buttons::BT_RIDE] = std::make_unique<MapleButton>(main["BtRide"]);
		buttons[Buttons::BT_MACRO] = std::make_unique<MapleButton>(main["BtMacro"]);

		buttons[Buttons::BT_HYPER]->set_state(Button::State::DISABLED);
		buttons[Buttons::BT_GUILDSKILL]->set_state(Button::State::DISABLED);
		buttons[Buttons::BT_RIDE]->set_state(Button::State::DISABLED);

		nl::node skillPoint = nl::nx::ui["UIWindow4.img"]["Skill"]["skillPoint"];

		sp_backgrnd = skillPoint["backgrnd"];
		sp_backgrnd2 = skillPoint["backgrnd2"];
		sp_backgrnd3 = skillPoint["backgrnd3"];

		buttons[Buttons::BT_CANCLE] = std::make_unique<MapleButton>(skillPoint["BtCancle"], Point<int16_t>(bg_dimensions.x(), 0));
		buttons[Buttons::BT_OKAY] = std::make_unique<MapleButton>(skillPoint["BtOkay"], Point<int16_t>(bg_dimensions.x(), 0));
		buttons[Buttons::BT_SPDOWN] = std::make_unique<MapleButton>(skillPoint["BtSpDown"], Point<int16_t>(bg_dimensions.x(), 0));
		buttons[Buttons::BT_SPMAX] = std::make_unique<MapleButton>(skillPoint["BtSpMax"], Point<int16_t>(bg_dimensions.x(), 0));
		buttons[Buttons::BT_SPUP] = std::make_unique<MapleButton>(skillPoint["BtSpUp"], Point<int16_t>(bg_dimensions.x(), 0));

		buttons[Buttons::BT_SPDOWN]->set_state(Button::State::DISABLED);

		sp_before = Charset(skillPoint["num"], Charset::Alignment::RIGHT);
		sp_after = Charset(skillPoint["num"], Charset::Alignment::RIGHT);
		sp_used = Text(Text::Font::A12B, Text::Alignment::RIGHT, Color::Name::WHITE);
		sp_remaining = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::SUPERNOVA);
		sp_name = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE);

		sprites.emplace_back(ui_backgrnd, Point<int16_t>(1, 0));
		sprites.emplace_back(main["backgrnd2"]);
		sprites.emplace_back(main["backgrnd3"]);

		nl::node macro = Skill["macro"];

		macro_backgrnd = macro["backgrnd"];
		macro_backgrnd2 = macro["backgrnd2"];
		macro_backgrnd3 = macro["backgrnd3"];

		buttons[Buttons::BT_MACRO_OK] = std::make_unique<MapleButton>(macro["BtOK"], Point<int16_t>(bg_dimensions.x(), 0));

		buttons[Buttons::BT_MACRO_OK]->set_state(Button::State::DISABLED);

		nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];

		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(close, Point<int16_t>(bg_dimensions.x() - 23, 6));

		nl::node Tab = main["Tab"];
		nl::node enabled = Tab["enabled"];
		nl::node disabled = Tab["disabled"];

		for (uint16_t i = Buttons::BT_TAB0; i <= Buttons::BT_TAB4; ++i)
		{
			uint16_t tabid = i - Buttons::BT_TAB0;
			buttons[i] = std::make_unique<TwoSpriteButton>(disabled[tabid], enabled[tabid]);
		}

		for (uint16_t i = Buttons::BT_SPUP0; i < Buttons::BT_SPUP0 + ROWS; ++i)
		{
			uint16_t row = i - Buttons::BT_SPUP0;
			Point<int16_t> spup_position = SKILL_OFFSET + Point<int16_t>(124, 20 + row * ROW_HEIGHT);

			buttons[i] = std::make_unique<MapleButton>(main["BtSpUp"], spup_position);
		}

		// The rows past the sixth belong to the two-column layout this window has
		// no room for. They exist so the button ids stay contiguous, and are left
		// inactive rather than sitting off the edge where they cannot be clicked.
		for (uint16_t i = Buttons::BT_SPUP0 + ROWS; i <= Buttons::BT_SPUP11; ++i)
		{
			buttons[i] = std::make_unique<MapleButton>(main["BtSpUp"], Point<int16_t>(0, 0));
			buttons[i]->set_active(false);
		}

		booktext = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, "", 150);
		splabel = Text(Text::Font::A12M, Text::Alignment::RIGHT, Color::Name::BLACK);

		slider = Slider(
			Slider::Type::DEFAULT,
			Range<int16_t>(SKILL_OFFSET.y(), SKILL_OFFSET.y() + LIST_HEIGHT),
			SKILL_OFFSET.x() + 145, ROWS, 1,
			[&](bool upwards)
			{
				int16_t shift = upwards ? -1 : 1;
				bool above = offset + shift >= 0;
				bool below = offset + ROWS + shift <= skillcount;

				if (above && below)
					change_offset(offset + shift);
			}
		);

		change_job(stats.get_stat(Maplestat::Id::JOB));

		set_macro(false);
		set_skillpoint(false);

		dimension = bg_dimensions;
		dragarea = Point<int16_t>(dimension.x(), 20);
	}

	int16_t UISkillbook::panel_height() const
	{
		size_t rows = (skills.size() + P_COLS - 1) / P_COLS;

		if (rows < 1)
			rows = 1;

		return static_cast<int16_t>(
			P_GRID_TOP + rows * P_CELL_H + P_ACTION_H + 20);
	}

	void UISkillbook::relayout_panel()
	{
		if (!panel)
			return;

		// The column above needs to know how tall this got, or the scroll
		// stops short of the action bar.
		dimension = Point<int16_t>(panel_screen.x(), panel_height());
	}

	Rectangle<int16_t> UISkillbook::panel_tab_box(uint16_t tabid) const
	{
		// Five tabs sharing the width, the way the quest log's three do.
		// Inside the gauges. They run the full height of both edges, and a tab
		// drawn under one is a tab you cannot press.
		int16_t w = static_cast<int16_t>((panel_screen.x() - P_EDGE * 2) / 5);
		int16_t x = static_cast<int16_t>(P_EDGE + tabid * w);

		return Rectangle<int16_t>(
			Point<int16_t>(x, P_TAB_TOP),
			Point<int16_t>(static_cast<int16_t>(x + w - 2),
				static_cast<int16_t>(P_TAB_TOP + P_TAB_H)));
	}

	Rectangle<int16_t> UISkillbook::panel_cell_box(size_t index) const
	{
		int16_t left = static_cast<int16_t>((panel_screen.x() - P_COLS * P_CELL_W) / 2);

		int16_t col = static_cast<int16_t>(index % P_COLS);
		int16_t row = static_cast<int16_t>(index / P_COLS);

		int16_t x = static_cast<int16_t>(left + col * P_CELL_W + 10);
		int16_t y = static_cast<int16_t>(P_GRID_TOP + row * P_CELL_H);

		return Rectangle<int16_t>(
			Point<int16_t>(x, y),
			Point<int16_t>(static_cast<int16_t>(x + 32),
				static_cast<int16_t>(y + 32)));
	}

	// THE ACTION ROW RUNS FROM THE LEFT, and stops well short of the right.
	//
	// It used to be measured back from the right edge, which put SPEND at
	// x226-326 - exactly where the panel pins its own "TO HOTKEYS" button.
	// SPEND was drawn, underneath it. Two things that belong to different
	// layers must not be laid out from the same edge.
	Rectangle<int16_t> UISkillbook::panel_action_box() const
	{
		int16_t top = static_cast<int16_t>(panel_height() - P_ACTION_H - 8);

		return Rectangle<int16_t>(
			Point<int16_t>(P_EDGE, top),
			Point<int16_t>(static_cast<int16_t>(P_EDGE + 88),
				static_cast<int16_t>(top + P_ACTION_H)));
	}

	Rectangle<int16_t> UISkillbook::panel_minus_box() const
	{
		int16_t top = static_cast<int16_t>(panel_height() - P_ACTION_H - 8);

		return Rectangle<int16_t>(
			Point<int16_t>(static_cast<int16_t>(P_EDGE + 100), top),
			Point<int16_t>(static_cast<int16_t>(P_EDGE + 128),
				static_cast<int16_t>(top + P_ACTION_H)));
	}

	Rectangle<int16_t> UISkillbook::panel_plus_box() const
	{
		int16_t top = static_cast<int16_t>(panel_height() - P_ACTION_H - 8);

		return Rectangle<int16_t>(
			Point<int16_t>(static_cast<int16_t>(P_EDGE + 156), top),
			Point<int16_t>(static_cast<int16_t>(P_EDGE + 184),
				static_cast<int16_t>(top + P_ACTION_H)));
	}

	int16_t UISkillbook::panel_tab_at(Point<int16_t> at) const
	{
		for (uint16_t t = 0; t < 5; t++)
			if (panel_tab_box(t).contains(at))
				return static_cast<int16_t>(t);

		return -1;
	}

	int16_t UISkillbook::panel_cell_at(Point<int16_t> at) const
	{
		for (size_t i = 0; i < skills.size(); i++)
		{
			// A finger, not a mouse: the whole cell counts, not just the
			// 32 pixels the icon occupies.
			Rectangle<int16_t> box = panel_cell_box(i);
			Rectangle<int16_t> reach(
				Point<int16_t>(box.left() - 8, box.top() - 4),
				Point<int16_t>(box.right() + 8, box.bottom() + 12));

			if (reach.contains(at))
				return static_cast<int16_t>(i);
		}

		return -1;
	}

	void UISkillbook::draw_panel(float alpha) const
	{
		if (panel_tab_text.get_text().empty())
		{
			panel_tab_text = Text(Text::Font::A11M, Text::Alignment::CENTER,
				Color::Name::WHITE);
			panel_level_text = Text(Text::Font::A11M, Text::Alignment::CENTER,
				Color::Name::WHITE);
			panel_name_text = Text(Text::Font::A12M, Text::Alignment::LEFT,
				Color::Name::WHITE);
			panel_action_text = Text(Text::Font::A11B, Text::Alignment::CENTER,
				Color::Name::WHITE);
		}

		// THE TABS, NAMED.
		//
		// "Beginner", "Warrior", "Fighter" - the real job names, out of
		// JobData, not "Tab 1". Same shape as the quest log's tab row so the
		// two read as the same control.
		for (uint16_t t = 0; t < 5; t++)
		{
			Rectangle<int16_t> box = panel_tab_box(t);
			bool here = (t == tab);

			GraphicsGL::get().drawrectangle(
				position.x() + box.left(), position.y() + box.top(),
				box.width(), box.height(),
				here ? 0.86f : 1.0f, here ? 0.74f : 1.0f, here ? 0.36f : 1.0f,
				here ? 0.55f : 0.14f);

			// ROMAN NUMERALS.
			//
			// The job names were tried and were wrong twice over: on a
			// character who has not advanced, get_subjob returns the SAME job
			// for every tab, so all five read "Beginner" - and the real names
			// are long enough to run into each other in a fifth of 344 pixels
			// anyway. I/II/III/IV/V fit, never collide, and cannot be wrong.
			static const char* NUMERAL[] = { "I", "II", "III", "IV", "V" };

			panel_tab_text.change_text(NUMERAL[t]);
			panel_tab_text.draw(position + Point<int16_t>(
				static_cast<int16_t>(box.left() + box.width() / 2),
				static_cast<int16_t>(box.top() + 2)));
		}

		// THE GRID. The same faint box the equipment rack and the bag draw,
		// with the icon in it and the level under it.
		for (size_t i = 0; i < skills.size(); i++)
		{
			Rectangle<int16_t> box = panel_cell_box(i);

			Point<int16_t> at = position + Point<int16_t>(box.left(), box.top());

			GraphicsGL::get().drawrectangle(
				at.x(), at.y(), 32, 32, 1.0f, 1.0f, 1.0f, 0.14f);

			if (static_cast<int16_t>(i) == panel_selected)
				GraphicsGL::get().drawrectangle(
					at.x() + 1, at.y() + 1, 32, 32, 1.0f, 0.92f, 0.45f, 0.85f);

			if (!check_required(skills[i].get_id()))
				skills[i].get_icon()->set_state(StatefulIcon::State::DISABLED);

			// THE ICON ONLY.
			//
			// SkillDisplayMeta::draw paints the icon AND the skill's name and
			// level 38 pixels to its right - which is correct for the book's
			// list, where a row is 140 wide and has nothing beside it, and
			// wrong here, where the next icon is 52 pixels away. Every name
			// was being written across its neighbour. That is the bunching.
			skills[i].get_icon()->draw(at);

			// The level under the box, where equipment puts the slot's name.
			panel_level_text.change_text(
				std::to_string(skills[i].get_level()) + "/"
				+ std::to_string(SkillData::get(skills[i].get_id()).get_masterlevel()));

			panel_level_text.draw(at + Point<int16_t>(16, 32));
		}

		// THE ACTION BAR: what is picked, how many points, and SPEND.
		//
		// This IS the "how many points" dialog, rebuilt at the panel's size.
		// The original is 200 pixels of artwork that opens to the RIGHT of a
		// 174-wide book - 374 across, on a screen 344 wide - which is why it
		// never appeared here however often the arrow was pressed.
		int16_t sp = spare_sp();
		bool ready = panel_selected >= 0
			&& panel_selected < static_cast<int16_t>(skills.size())
			&& sp >= panel_spend;

		if (panel_selected >= 0 && panel_selected < static_cast<int16_t>(skills.size()))
		{
			const SkillData& data = SkillData::get(skills[panel_selected].get_id());

			panel_name_text.change_text(data.get_name());
		}
		else
		{
			panel_name_text.change_text("Pick a skill");
		}

		int16_t bar_y = static_cast<int16_t>(panel_screen.y() - P_ACTION_H - 16);

		// The name and the SP go on their OWN line, above the controls - the
		// row below is four boxes wide and there is no room beside them.
		panel_name_text.draw(position + Point<int16_t>(P_EDGE, bar_y - 20));

		panel_level_text.change_text(std::to_string(sp) + " SP LEFT");
		panel_level_text.draw(position + Point<int16_t>(
			static_cast<int16_t>(P_EDGE + 250), bar_y - 20));

		auto chip = [&](Rectangle<int16_t> box, const char* label, bool live)
		{
			GraphicsGL::get().drawrectangle(
				position.x() + box.left(), position.y() + box.top(),
				box.width(), box.height(),
				live ? 0.30f : 0.16f, live ? 0.34f : 0.16f,
				live ? 0.42f : 0.16f, live ? 0.92f : 0.45f);

			panel_action_text.change_text(label);
			panel_action_text.draw(position + Point<int16_t>(
				static_cast<int16_t>(box.left() + box.width() / 2),
				static_cast<int16_t>(box.top() + 4)));
		};

		chip(panel_minus_box(), "-", panel_spend > 1);
		chip(panel_plus_box(), "+", panel_spend < sp);

		// The count, between the two.
		panel_level_text.change_text(std::to_string(panel_spend));
		panel_level_text.draw(position + Point<int16_t>(
			static_cast<int16_t>(P_EDGE + 142), bar_y + 4));

		Rectangle<int16_t> act = panel_action_box();

		GraphicsGL::get().drawrectangle(
			position.x() + act.left(), position.y() + act.top(),
			act.width(), act.height(),
			ready ? 0.18f : 0.10f,
			ready ? 0.44f : 0.16f,
			ready ? 0.20f : 0.10f,
			ready ? 0.92f : 0.55f);

		panel_action_text.change_text("SPEND");
		panel_action_text.draw(position + Point<int16_t>(
			static_cast<int16_t>(act.left() + act.width() / 2),
			static_cast<int16_t>(act.top() + 5)));

		(void)alpha;
	}

	bool UISkillbook::panel_pressed(Point<int16_t> at)
	{
		if (int16_t t = panel_tab_at(at); t >= 0)
		{
			if (t != static_cast<int16_t>(tab))
			{
				change_tab(static_cast<uint16_t>(t));
				panel_selected = -1;
				panel_spend = 1;
			}

			return true;
		}

		if (int16_t cell = panel_cell_at(at); cell >= 0)
		{
			panel_selected = cell;
			panel_spend = 1;

			return true;
		}

		int16_t sp = spare_sp();

		if (panel_minus_box().contains(at))
		{
			if (panel_spend > 1)
				panel_spend--;

			return true;
		}

		if (panel_plus_box().contains(at))
		{
			if (panel_spend < sp)
				panel_spend++;

			return true;
		}

		if (panel_action_box().contains(at))
		{
			if (panel_selected < 0
				|| panel_selected >= static_cast<int16_t>(skills.size())
				|| sp < panel_spend)
				return true;

			int32_t id = skills[panel_selected].get_id();

			// One packet per point - v83 has no "spend n" message, the level
			// goes up by one each time. Sent in a row and the UI locked ONCE
			// at the end, rather than locking between each and dropping the
			// rest on the floor.
			for (int16_t i = 0; i < panel_spend; i++)
				SpendSpPacket(id).dispatch();

			panel_spend = 1;

			UI::get().disable();

			return true;
		}

		return false;
	}

	void UISkillbook::set_panel(Point<int16_t> screen)
	{
		panel = true;
		panel_screen = screen;

		// AS WIDE AS THE PANEL, AS TALL AS ITS OWN CONTENT.
		//
		// The book is 174 wide and the panel is 344, so a window left at its
		// own size sat in the middle with a third of the screen empty either
		// side - and the spend-points dialog, which opens to the RIGHT of it,
		// opened off the edge. That is why it never appeared.
		relayout_panel();

		// EVERY BORROWED BUTTON OFF.
		//
		// The book's twelve tiny spend arrows, its tabs and its close box are
		// all placed against a 174-wide page of artwork that is no longer
		// drawn here. They were still taking touches from wherever they
		// happened to land. The panel draws its own tabs, cells and action
		// bar and hit-tests them itself - see panel_pressed.
		for (auto& entry : buttons)
			if (entry.second)
				entry.second->set_active(false);
	}

	bool UISkillbook::indragrange(Point<int16_t> cursorpos) const
	{
		if (panel)
			return false;

		return UIDragElement::indragrange(cursorpos);
	}

	void UISkillbook::draw(float alpha) const
	{
		// DECONSTRUCTED ON THE PANEL, like every other page.
		//
		// This was the last window still drawing the game's own artwork - a
		// book, faded to 0.6 - while the inventory, the equipment rack, the
		// stat sheet and the quest log had all been reduced to plain cells on
		// the panel's frame. One page in a book and five on parchment is the
		// whole of why the skills looked like they belonged somewhere else.
		//
		// The rows draw their own faint boxes below; nothing else is needed.
		if (panel)
		{
			draw_panel(alpha);

			return;
		}

		if (!panel)
		{
			UIElement::draw_sprites(alpha);

			// These three were placed against the wider window - the book name
			// at x 173 and the SP count at x 304, both outside a window 174
			// across, so neither was ever visible.
			bookicon.draw(position + Point<int16_t>(11, 26));
			booktext.draw(position + Point<int16_t>(87, 30));
			splabel.draw(position + Point<int16_t>(165, 248));
		}
		else
		{
			// The one number that matters here, on the row the panel keeps
			// free above the list.
			splabel.draw(position + Point<int16_t>(6, 4));
		}

		Point<int16_t> pos = position + SKILL_OFFSET + Point<int16_t>(-1, 0);

		for (size_t i = 0; i < ROWS; i++)
		{
			// The list shows a window onto the skills, so a row draws whichever
			// skill the scroll offset puts there - not the i'th one, which is why
			// scrolling used to move the bar without moving the contents.
			size_t index = offset + i;

			if (index < skills.size())
			{
				// THE SAME FAINT BOX THE OTHER GRIDS DRAW.
				//
				// 32x32 at 0.14, which is what the equipment page uses for a
				// worn slot and what the inventory now uses for a bag slot.
				// On the panel the row plates (skille/skilld/skillb) are not
				// drawn at all - they are pieces of the book.
				if (panel)
				{
					GraphicsGL::get().drawrectangle(
						pos.x(), pos.y(), 32, 32, 1.0f, 1.0f, 1.0f, 0.14f);

					if (!check_required(skills[index].get_id()))
						skills[index].get_icon()->set_state(
							StatefulIcon::State::DISABLED);
				}
				else if (check_required(skills[index].get_id()))
				{
					skille.draw(pos);
				}
				else
				{
					skilld.draw(pos);
					skills[index].get_icon()->set_state(StatefulIcon::State::DISABLED);
				}

				skills[index].draw(pos + SKILL_META_OFFSET);
			}
			else if (!panel)
			{
				skillb.draw(pos);
			}

			// The divider is part of the book's page. On the panel the gap
			// between the boxes is the divider.
			if (i < ROWS - 1 && !panel)
				line.draw(pos + LINE_OFFSET);

			pos.shift_y(ROW_HEIGHT);
		}

		// The spend-a-point panel. Its three layers were loaded and measured -
		// set_skillpoint widens the window by them - but never actually painted,
		// so the panel was invisible and only its buttons showed, floating over
		// the map. Nothing reached it before because the arrow that opens it was
		// the one sitting outside the window.
		//
		// It sits to the right of the skill list, which is where the buttons were
		// already placed. backgrnd2 and backgrnd3 carry their own offsets as
		// origins, so all three draw from the same corner.
		if (sp_enabled)
		{
			Point<int16_t> sp_pos = position + Point<int16_t>(bg_dimensions.x(), 0);

			// Faint like the book it opens beside.
			float fade = panel ? PANEL_FADE : 1.0f;

			sp_backgrnd.draw(DrawArgument(sp_pos, fade));
			sp_backgrnd2.draw(DrawArgument(sp_pos, fade));
			sp_backgrnd3.draw(DrawArgument(sp_pos, fade));

			// The artwork supplies the labels and the empty boxes; these are
			// the values that go in them, measured against it.
			sp_skill.draw(sp_pos + Point<int16_t>(23, 39));
			sp_name.draw(sp_pos + Point<int16_t>(87, 46));

			// After "REMAINING SP :" and between "SP TO USE :" and the + button.
			sp_remaining.draw(sp_pos + Point<int16_t>(74, 67));
			sp_used.draw(sp_pos + Point<int16_t>(86, 88));

			// The two white boxes either side of the arrow. Both charsets are
			// right aligned, so these are the right-hand edges.
			sp_before.draw(sp_before_text, sp_pos + Point<int16_t>(74, 151));
			sp_after.draw(sp_after_text, sp_pos + Point<int16_t>(151, 151));
		}

		UIElement::draw_buttons(alpha);
	}

	Button::State UISkillbook::button_pressed(uint16_t id)
	{
		int16_t cur_sp = spare_sp();

		switch (id)
		{
		case Buttons::BT_CLOSE:
			close();
			break;
		case Buttons::BT_MACRO:
			set_macro(!macro_enabled);
			break;
		case Buttons::BT_CANCLE:
			set_skillpoint(false);
			break;
		case Buttons::BT_OKAY:
		{
			int32_t used = std::stoi(sp_used.get_text());

			while (used > 0)
			{
				spend_sp(sp_id);
				used--;
			}

			change_sp();
			set_skillpoint(false);
		}
		break;
		case Buttons::BT_SPDOWN:
		{
			int32_t used = std::stoi(sp_used.get_text());
			int32_t sp_after = std::stoi(sp_after_text);
			int32_t sp_before = std::stoi(sp_before_text);
			used--;
			sp_after--;

			sp_after_text = std::to_string(sp_after);
			sp_used.change_text(std::to_string(used));
			sp_remaining.change_text(std::to_string(cur_sp - used));

			buttons[Buttons::BT_SPUP]->set_state(Button::State::NORMAL);
			buttons[Buttons::BT_SPMAX]->set_state(Button::State::NORMAL);

			if (sp_after - 1 == sp_before)
				return Button::State::DISABLED;

			return Button::State::NORMAL;
		}
		break;
		case Buttons::BT_SPMAX:
		{
			int32_t used = std::stoi(sp_used.get_text());
			int32_t sp_before = std::stoi(sp_before_text);
			int32_t sp_touse = sp_masterlevel - sp_before - used;

			used += sp_touse;

			sp_after_text = std::to_string(sp_masterlevel);
			sp_used.change_text(std::to_string(used));
			sp_remaining.change_text(std::to_string(cur_sp - used));

			buttons[Buttons::BT_SPUP]->set_state(Button::State::DISABLED);
			buttons[Buttons::BT_SPDOWN]->set_state(Button::State::NORMAL);

			return Button::State::DISABLED;
		}
		break;
		case Buttons::BT_SPUP:
		{
			int32_t used = std::stoi(sp_used.get_text());
			int32_t sp_after = std::stoi(sp_after_text);
			used++;
			sp_after++;

			sp_after_text = std::to_string(sp_after);
			sp_used.change_text(std::to_string(used));
			sp_remaining.change_text(std::to_string(cur_sp - used));

			buttons[Buttons::BT_SPDOWN]->set_state(Button::State::NORMAL);

			if (sp_after == sp_masterlevel)
			{
				buttons[Buttons::BT_SPMAX]->set_state(Button::State::DISABLED);

				return Button::State::DISABLED;
			}

			return Button::State::NORMAL;
		}
		break;
		case Buttons::BT_TAB0:
		case Buttons::BT_TAB1:
		case Buttons::BT_TAB2:
		case Buttons::BT_TAB3:
		case Buttons::BT_TAB4:
			change_tab(id - Buttons::BT_TAB0);

			return Button::State::PRESSED;
		case Buttons::BT_SPUP0:
		case Buttons::BT_SPUP1:
		case Buttons::BT_SPUP2:
		case Buttons::BT_SPUP3:
		case Buttons::BT_SPUP4:
		case Buttons::BT_SPUP5:
		case Buttons::BT_SPUP6:
		case Buttons::BT_SPUP7:
		case Buttons::BT_SPUP8:
		case Buttons::BT_SPUP9:
		case Buttons::BT_SPUP10:
		case Buttons::BT_SPUP11:
			send_spup(id - Buttons::BT_SPUP0 + offset);
			break;
		case Buttons::BT_HYPER:
		case Buttons::BT_GUILDSKILL:
		case Buttons::BT_RIDE:
		case Buttons::BT_MACRO_OK:
		default:
			break;
		}

		return Button::State::NORMAL;
	}

	void UISkillbook::toggle_active()
	{
		if (!is_skillpoint_enabled())
		{
			UIElement::toggle_active();

			clear_tooltip();
		}
	}

	void UISkillbook::doubleclick(Point<int16_t> cursorpos)
	{
		const SkillDisplayMeta* skill = skill_by_position(cursorpos - position);

		if (skill)
		{
			int32_t skill_id = skill->get_id();
			int32_t skill_level = skillbook.get_level(skill_id);

			if (skill_level > 0)
				Stage::get().get_combat().use_move(skill_id);
		}
	}

	void UISkillbook::remove_cursor()
	{
		UIDragElement::remove_cursor();

		slider.remove_cursor();
	}

	void UISkillbook::send_scroll(double yoffset)
	{
		if (slider.isenabled())
			slider.send_scroll(yoffset);
	}

	Cursor::State UISkillbook::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// THE PANEL PAGE TAKES ITS OWN TOUCHES.
		//
		// Everything below this belongs to the book: dragging the window,
		// its slider, and picking a skill icon up to put on the quickslot
		// bar. None of it is placed where the panel draws, so letting it run
		// first is how a press meant for a tab became a drag of an icon that
		// was not under the finger.
		if (panel)
		{
			if (clicked)
				panel_pressed(cursorpos - position);

			return Cursor::State::IDLE;
		}

		Cursor::State dstate = UIDragElement::send_cursor(clicked, cursorpos);

		if (dragged)
			return dstate;

		Point<int16_t> cursor_relative = cursorpos - position;

		if (slider.isenabled())
		{
			if (Cursor::State new_state = slider.send_cursor(cursor_relative, clicked))
			{
				clear_tooltip();

				return new_state;
			}
		}

		if (!grabbing)
		{
			for (size_t i = 0; i < ROWS && offset + i < skills.size(); i++)
			{
				Point<int16_t> skill_position = position + SKILL_OFFSET
					+ Point<int16_t>(-1, static_cast<int16_t>(i * ROW_HEIGHT));

				constexpr Rectangle<int16_t> bounds = Rectangle<int16_t>(0, 32, 0, 32);
				bool inrange = bounds.contains(cursorpos - skill_position);

				if (inrange)
				{
					size_t index = offset + i;

					if (clicked)
					{
						clear_tooltip();
						grabbing = true;

						int32_t skill_id = skills[index].get_id();
						int32_t skill_level = skillbook.get_level(skill_id);

						if (skill_level > 0 && !SkillData::get(skill_id).is_passive())
						{
							skills[index].get_icon()->start_drag(cursorpos - skill_position);
							UI::get().drag_icon(skills[index].get_icon());

							return Cursor::State::GRABBING;
						}
						else
						{
							return Cursor::State::IDLE;
						}
					}
					else
					{
						skills[index].get_icon()->set_state(StatefulIcon::State::MOUSEOVER);
						show_skill(skills[index].get_id());

						return Cursor::State::IDLE;
					}
				}
			}

			for (size_t i = 0; i < skills.size(); i++)
			{
				skills[i].get_icon()->set_state(StatefulIcon::State::NORMAL);
			}
			clear_tooltip();
		}
		else
		{
			grabbing = false;
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}

	void UISkillbook::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				if (sp_enabled)
					set_skillpoint(false);
				else
					close();
			}
			else if (keycode == KeyAction::Id::TAB)
			{
				clear_tooltip();

				Job::Level level = job.get_level();
				uint16_t id = tab + 1;
				uint16_t new_tab = tab + Buttons::BT_TAB0;

				if (new_tab < Buttons::BT_TAB4 && id <= level)
					new_tab++;
				else
					new_tab = Buttons::BT_TAB0;

				change_tab(new_tab - Buttons::BT_TAB0);
			}
		}
	}

	UIElement::Type UISkillbook::get_type() const
	{
		return TYPE;
	}

	void UISkillbook::update_stat(Maplestat::Id stat, int16_t value)
	{
		switch (stat)
		{
		case Maplestat::Id::JOB:
			change_job(value);
			break;
		case Maplestat::Id::SP:
			change_sp();
			break;
		}
	}

	void UISkillbook::update_skills(int32_t skill_id)
	{
		change_tab(tab);
	}

	void UISkillbook::change_job(uint16_t id)
	{
		job.change_job(id);

		Job::Level level = job.get_level();

		for (uint16_t i = 0; i <= Job::Level::FOURTH; i++)
			buttons[Buttons::BT_TAB0 + i]->set_active(i <= level);

		change_tab(level - Job::Level::BEGINNER);
	}

	int16_t UISkillbook::spare_sp() const
	{
		// Straight from the members change_sp() maintains, rather than parsed
		// back out of the text drawn from them. Beginners spend a separate
		// pool that is worked out here rather than sent by the server.
		return joblevel_by_tab(tab) == Job::Level::BEGINNER ? beginner_sp : sp;
	}

	void UISkillbook::change_sp()
	{
		Job::Level joblevel = joblevel_by_tab(tab);
		uint16_t level = stats.get_stat(Maplestat::Id::LEVEL);

		if (joblevel == Job::Level::BEGINNER)
		{
			int16_t remaining_beginner_sp = 0;

			if (level >= 7)
				remaining_beginner_sp = 6;
			else
				remaining_beginner_sp = level - 1;

			for (size_t i = 0; i < skills.size(); i++)
			{
				int32_t skillid = skills[i].get_id();

				if (skillid == SkillId::Id::THREE_SNAILS || skillid == SkillId::Id::HEAL || skillid == SkillId::Id::FEATHER)
					remaining_beginner_sp -= skills[i].get_level();
			}

			beginner_sp = remaining_beginner_sp;
			splabel.change_text(std::to_string(beginner_sp));
		}
		else
		{
			sp = stats.get_stat(Maplestat::Id::SP);
			splabel.change_text(std::to_string(sp));
		}

		change_offset(offset);
		set_skillpoint(false);
	}

	void UISkillbook::change_tab(uint16_t new_tab)
	{
		buttons[Buttons::BT_TAB0 + tab]->set_state(Button::NORMAL);
		buttons[Buttons::BT_TAB0 + new_tab]->set_state(Button::PRESSED);
		tab = new_tab;

		skills.clear();
		skillcount = 0;

		Job::Level joblevel = joblevel_by_tab(tab);
		uint16_t subid = job.get_subjob(joblevel);

		const JobData& data = JobData::get(subid);

		bookicon = data.get_icon();
		booktext.change_text(data.get_name());

		for (int32_t skill_id : data.get_skills())
		{
			int32_t level = skillbook.get_level(skill_id);
			int32_t masterlevel = skillbook.get_masterlevel(skill_id);

			bool invisible = SkillData::get(skill_id).is_invisible();

			if (invisible && masterlevel == 0)
				continue;

			skills.emplace_back(skill_id, level);
			skillcount++;
		}

		slider.setrows(ROWS, skillcount);
		change_offset(0);
		change_sp();

		// The grid grew or shrank, so the page did too.
		relayout_panel();
	}

	void UISkillbook::change_offset(uint16_t new_offset)
	{
		offset = new_offset;

		for (int16_t i = 0; i < ROWS; i++)
		{
			uint16_t index = Buttons::BT_SPUP0 + i;
			uint16_t row = offset + i;
			buttons[index]->set_active(row < skillcount);

			if (row < skills.size())
			{
				int32_t skill_id = skills[row].get_id();
				bool canraise = can_raise(skill_id);
				buttons[index]->set_state(canraise ? Button::State::NORMAL : Button::State::DISABLED);
			}
		}
	}

	void UISkillbook::show_skill(int32_t id)
	{
		int32_t skill_id = id;
		int32_t level = skillbook.get_level(id);
		int32_t masterlevel = skillbook.get_masterlevel(id);
		int64_t expiration = skillbook.get_expiration(id);

		UI::get().show_skill(Tooltip::Parent::SKILLBOOK, skill_id, level, masterlevel, expiration);
	}

	void UISkillbook::clear_tooltip()
	{
		UI::get().clear_tooltip(Tooltip::Parent::SKILLBOOK);
	}

	bool UISkillbook::can_raise(int32_t skill_id) const
	{
		Job::Level joblevel = joblevel_by_tab(tab);

		if (joblevel == Job::Level::BEGINNER && beginner_sp <= 0)
			return false;

		if (tab + Buttons::BT_TAB0 != Buttons::BT_TAB0 && sp <= 0)
			return false;

		int32_t level = skillbook.get_level(skill_id);
		int32_t masterlevel = skillbook.get_masterlevel(skill_id);

		if (masterlevel == 0)
			masterlevel = SkillData::get(skill_id).get_masterlevel();

		if (level >= masterlevel)
			return false;

		switch (skill_id)
		{
		case SkillId::Id::ANGEL_BLESSING:
			return false;
		default:
			return check_required(skill_id);
		}
	}

	void UISkillbook::send_spup(uint16_t row)
	{
		if (row >= skills.size())
			return;

		int32_t id = skills[row].get_id();

		if (sp_enabled && id == sp_id)
		{
			set_skillpoint(false);
			return;
		}

		int32_t level = skills[row].get_level();
		int32_t used = 1;

		const SkillData& skillData = SkillData::get(id);
		std::string name = skillData.get_name();
		int16_t cur_sp = spare_sp();

		sp_before_text = std::to_string(level);
		sp_after_text = std::to_string(level + used);
		sp_used.change_text(std::to_string(used));
		sp_remaining.change_text(std::to_string(cur_sp - used));
		sp_name.change_text(name);
		sp_skill = skills[row].get_icon()->get_texture();
		sp_id = id;
		sp_masterlevel = skillData.get_masterlevel();

		if (sp_masterlevel == 1)
		{
			buttons[Buttons::BT_SPDOWN]->set_state(Button::State::DISABLED);
			buttons[Buttons::BT_SPMAX]->set_state(Button::State::DISABLED);
			buttons[Buttons::BT_SPUP]->set_state(Button::State::DISABLED);
		}
		else
		{
			buttons[Buttons::BT_SPDOWN]->set_state(Button::State::DISABLED);
			buttons[Buttons::BT_SPMAX]->set_state(Button::State::NORMAL);
			buttons[Buttons::BT_SPUP]->set_state(Button::State::NORMAL);
		}

		if (!sp_enabled)
			set_skillpoint(true);
	}

	void UISkillbook::spend_sp(int32_t skill_id)
	{
		SpendSpPacket(skill_id).dispatch();

		UI::get().disable();
	}

	Job::Level UISkillbook::joblevel_by_tab(uint16_t t) const
	{
		switch (t)
		{
		case 1:
			return Job::Level::FIRST;
		case 2:
			return Job::Level::SECOND;
		case 3:
			return Job::Level::THIRD;
		case 4:
			return Job::Level::FOURTH;
		default:
			return Job::Level::BEGINNER;
		}
	}

	const UISkillbook::SkillDisplayMeta* UISkillbook::skill_by_position(Point<int16_t> cursorpos) const
	{
		int16_t x = cursorpos.x();

		// A row is the width of the artwork, 140.
		if (x < SKILL_OFFSET.x() || x > SKILL_OFFSET.x() + ROW_ART_WIDTH)
			return nullptr;

		int16_t y = cursorpos.y();

		if (y < SKILL_OFFSET.y())
			return nullptr;

		int16_t row = (y - SKILL_OFFSET.y()) / ROW_HEIGHT;

		if (row < 0 || row >= ROWS)
			return nullptr;

		// The row is a position in the list, so the scroll offset decides which
		// skill it actually is. Comparing that against ROWS rather than against
		// how many skills there are is what hid everything past the sixth.
		size_t skill_idx = static_cast<size_t>(offset) + row;

		if (skill_idx >= skills.size())
			return nullptr;

		auto iter = skills.data() + skill_idx;


		// TODO: (rich) see if works properly.
		return iter;
	}

	void UISkillbook::close()
	{
		clear_tooltip();
		deactivate();
	}

	bool UISkillbook::check_required(int32_t id) const
	{
		std::unordered_map<int32_t, int32_t> required = skillbook.collect_required(id);

		if (required.size() <= 0)
			required = SkillData::get(id).get_reqskills();

		for (auto reqskill : required)
		{
			int32_t reqskill_level = skillbook.get_level(reqskill.first);
			int32_t req_level = reqskill.second;

			if (reqskill_level < req_level)
				return false;
		}

		return true;
	}

	void UISkillbook::set_macro(bool enabled)
	{
		macro_enabled = enabled;

		if (macro_enabled)
			dimension = bg_dimensions + Point<int16_t>(macro_backgrnd.get_dimensions().x(), 0);
		else if (!sp_enabled)
			dimension = bg_dimensions;

		buttons[Buttons::BT_MACRO_OK]->set_active(macro_enabled);

		if (macro_enabled && sp_enabled)
			set_skillpoint(false);
	}

	void UISkillbook::set_skillpoint(bool enabled)
	{
		sp_enabled = enabled;

		if (sp_enabled)
			dimension = bg_dimensions + Point<int16_t>(sp_backgrnd.get_dimensions().x(), 0);
		else if (!macro_enabled)
			dimension = bg_dimensions;

		buttons[Buttons::BT_CANCLE]->set_active(sp_enabled);
		buttons[Buttons::BT_OKAY]->set_active(sp_enabled);
		buttons[Buttons::BT_SPDOWN]->set_active(sp_enabled);
		buttons[Buttons::BT_SPMAX]->set_active(sp_enabled);
		buttons[Buttons::BT_SPUP]->set_active(sp_enabled);

		if (sp_enabled && macro_enabled)
			set_macro(false);
	}

	bool UISkillbook::is_skillpoint_enabled()
	{
		return sp_enabled;
	}
}