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
#include "UIQuestHelper.h"

#include "../../Graphics/GraphicsGL.h"

#include "../../Data/QuestData.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/Geometry.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UIQuestHelper::UIQuestHelper() : UIDragElement<PosQUESTHELPER>(Point<int16_t>(WIDTH, 20))
	{
		// The same tooltip 9-slice the party panel uses, so the two read as
		// one family rather than two different games.
		frame = MapleFrame(nl::nx::ui["UIToolTip.img"]["Item"]["Frame2"]);

		title = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::WHITE);
		quest_name = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::YELLOW);
		row_text = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);
		row_done = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::LIGHTGREY);

		dimension = Point<int16_t>(WIDTH, TITLE_H + 14);
	}

	void UIQuestHelper::update()
	{
		UIElement::update();

		// Rebuilt every tick rather than cached. A kill count changes on the
		// server's word and there is no event to hang a refresh off, so the
		// only way for this to never be stale is to never keep it.
		entries.clear();

		const Questlog& log = Stage::get().get_player().get_quests();

		for (int16_t qid : log.all_started())
		{
			if (entries.size() >= MAX_QUESTS)
				break;

			const QuestData& data = QuestData::get(qid);

			Entry entry;
			entry.name = data.get_name();

			const QuestData::Requirements& need = data.to_finish();

			size_t which = 0;

			for (const auto& mob : need.mobs)
			{
				int16_t have = log.killed(qid, which);

				entry.rows.push_back({
					std::string(nl::nx::string["Mob.img"][std::to_string(mob.first)]["name"])
						+ "  " + std::to_string(have) + "/" + std::to_string(mob.second),
					have >= mob.second });

				which++;
			}

			for (const auto& item : need.items)
			{
				if (item.second <= 0)
					continue;

				// Items are counted from the inventory rather than the
				// progress string - the server does not report them.
				int16_t have = static_cast<int16_t>(
					Stage::get().get_player().get_inventory().get_total_item_count(item.first));

				entry.rows.push_back({
					std::string(nl::nx::string["Item.img"]["Etc"]
						[std::to_string(item.first)]["name"])
						+ "  " + std::to_string(have) + "/" + std::to_string(item.second),
					have >= item.second });
			}

			// A quest with nothing left to count is still worth listing - it
			// means "go and hand it in", which is exactly when people get
			// stuck.
			entries.push_back(std::move(entry));
		}

		int16_t rows = 0;
		for (const auto& e : entries)
			rows += static_cast<int16_t>(e.rows.size());

		int16_t h = static_cast<int16_t>(
			TITLE_H + entries.size() * NAME_H + rows * ROW_H + 14);

		dimension = Point<int16_t>(WIDTH, entries.empty() ? 0 : h);
	}

	Rectangle<int16_t> UIQuestHelper::close_box() const
	{
		// Top right of the plate, a thumb wide - this is tapped on a handheld,
		// not clicked.
		constexpr int16_t S = 22;

		return Rectangle<int16_t>(
			position + Point<int16_t>(WIDTH - S - 6, 4),
			position + Point<int16_t>(WIDTH - 6, 4 + S));
	}

	Cursor::State UIQuestHelper::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (clicked && !entries.empty() && close_box().contains(cursorpos))
		{
			deactivate();

			return Cursor::State::IDLE;
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIQuestHelper::draw(float inter) const
	{
		if (entries.empty())
			return;

		int16_t rows = 0;
		for (const auto& e : entries)
			rows += static_cast<int16_t>(e.rows.size());

		int16_t top_h = 7;
		int16_t inner_h = static_cast<int16_t>(
			TITLE_H + entries.size() * NAME_H + rows * ROW_H);
		int16_t panel_h = static_cast<int16_t>(top_h + inner_h + 10);

		Point<int16_t> tl = position;

		frame.draw(tl + Point<int16_t>(WIDTH / 2, panel_h - 6), WIDTH - 19, panel_h - 17);

		title.change_text("Quests");
		title.draw(tl + Point<int16_t>(10, top_h + 1));

		// The close box, drawn rather than built from artwork - there is no
		// button in the game's files that belongs on a plate this size.
		Rectangle<int16_t> shut = close_box();

		GraphicsGL::get().drawrectangle(
			shut.left(), shut.top(), shut.width(), shut.height(),
			0.55f, 0.16f, 0.16f, 0.85f);

		title.change_text("X");
		title.draw(Point<int16_t>(shut.left() + 7, shut.top() + 1));

		static const ColorBox divider(WIDTH - 10, 1, Color::Name::WHITE, 0.7f);
		divider.draw(DrawArgument(tl + Point<int16_t>(5, top_h + TITLE_H)));

		int16_t y = static_cast<int16_t>(top_h + TITLE_H + 5);

		for (const auto& e : entries)
		{
			quest_name.change_text(e.name);
			quest_name.draw(tl + Point<int16_t>(8, y));
			y += NAME_H;

			for (const auto& r : e.rows)
			{
				// Finished requirements go grey rather than vanishing, so the
				// list does not reshuffle under the reader's eye.
				Text& t = r.done ? row_done : row_text;
				t.change_text((r.done ? "* " : "- ") + r.text);
				t.draw(tl + Point<int16_t>(16, y));
				y += ROW_H;
			}
		}

		UIElement::draw(inter);
	}

	UIElement::Type UIQuestHelper::get_type() const
	{
		return TYPE;
	}
}
