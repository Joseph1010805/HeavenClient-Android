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
#include "UIQuestLog.h"

#include "../KeyAction.h"
#include "../UI.h"

#include "../Audio/Audio.h"
#include "../Data/ItemData.h"
#include "../Data/QuestData.h"
#include "../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../Net/Packets/QuestPackets.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UIQuestLog::UIQuestLog(const Questlog& ql) : UIDragElement<PosQUEST>(), questlog(ql)
	{
		heading = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE);
		rowtext = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);
		label = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE);

		// Wrapped to the window, since a journal entry is a paragraph.
		body = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::LIGHTGREY,
			"", WINDOW_W - 2 * PAD - 8);

		dimension = Point<int16_t>(WINDOW_W, WINDOW_H);
		dragarea = Point<int16_t>(WINDOW_W, 20);

		slider = Slider(
			Slider::Type::DEFAULT,
			Range<int16_t>(LIST_TOP, WINDOW_H - PAD),
			WINDOW_W - 18, 1, 1,
			[&](bool upwards)
			{
				int16_t shift = upwards ? -1 : 1;

				if (offset + shift >= 0
					&& offset + shift + rows_shown() <= static_cast<int16_t>(listed.size()))
					offset += shift;
			});

		rebuild();
	}

	void UIQuestLog::set_panel(Point<int16_t> screen)
	{
		panel = true;
		panel_screen = screen;

		dimension = Point<int16_t>(screen.x(), screen.y());
		body = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::LIGHTGREY,
			"", screen.x() - 2 * PAD - 8);

		slider = Slider(
			Slider::Type::DEFAULT,
			Range<int16_t>(LIST_TOP, screen.y() - PAD),
			screen.x() - 18, 1, 1,
			[&](bool upwards)
			{
				int16_t shift = upwards ? -1 : 1;

				if (offset + shift >= 0
					&& offset + shift + rows_shown() <= static_cast<int16_t>(listed.size()))
					offset += shift;
			});

		rebuild();
	}

	int16_t UIQuestLog::width() const { return dimension.x(); }
	int16_t UIQuestLog::height() const { return dimension.y(); }

	int16_t UIQuestLog::rows_shown() const
	{
		return (height() - LIST_TOP - PAD) / ROW_H;
	}

	bool UIQuestLog::showing_detail() const
	{
		return opened != 0;
	}

	const char* UIQuestLog::tab_name(Tab which)
	{
		switch (which)
		{
		case AVAILABLE:   return "AVAILABLE";
		case IN_PROGRESS: return "DOING";
		case COMPLETED:   return "DONE";
		default:          return "";
		}
	}

	void UIQuestLog::rebuild()
	{
		listed.clear();
		offset = 0;

		const Player& player = Stage::get().get_player();

		switch (tab)
		{
		case IN_PROGRESS:
			listed = questlog.all_started();
			break;
		case COMPLETED:
			listed = questlog.all_completed();
			break;
		case AVAILABLE:
		{
			// Every quest in the game is a candidate, so the level filter
			// runs first - see QuestData::candidates.
			int16_t level = static_cast<int16_t>(
				player.get_stats().get_stat(Maplestat::Id::LEVEL));

			for (int16_t questid : QuestData::candidates(level))
				if (player.quest_state(questid) == Questlog::State::AVAILABLE)
					listed.push_back(questid);

			break;
		}
		default:
			break;
		}

		int16_t rows = static_cast<int16_t>(listed.size());
		slider.setrows(0, rows_shown(), rows > 0 ? rows : 1);

		last_started = questlog.all_started().size();
		last_completed = questlog.all_completed().size();
	}

	void UIQuestLog::update()
	{
		UIElement::update();

		// A quest taken or handed in at an NPC changes this window without
		// anyone touching it.
		if (questlog.all_started().size() != last_started
			|| questlog.all_completed().size() != last_completed)
			rebuild();
	}

	void UIQuestLog::draw_plate(Point<int16_t> at, int16_t w, int16_t h) const
	{
		GraphicsGL::get().drawrectangle(at.x(), at.y(), w, h, 0.05f, 0.06f, 0.09f, 0.92f);
	}

	void UIQuestLog::draw(float inter) const
	{
		draw_plate(position, width(), height());

		heading.change_text("QUEST JOURNAL");
		heading.draw(position + Point<int16_t>(PAD, 3));

		// The tab row, across the top where a thumb can reach it.
		for (uint8_t t = 0; t < TAB_COUNT; t++)
		{
			Rectangle<int16_t> at = tab_bounds(static_cast<Tab>(t));
			bool here = (t == tab);

			GraphicsGL::get().drawrectangle(
				at.left(), at.top(), at.width(), at.height(),
				here ? 0.28f : 0.11f, here ? 0.30f : 0.12f, here ? 0.38f : 0.16f, 1.0f);

			label.change_text(tab_name(static_cast<Tab>(t)));
			label.draw(Point<int16_t>(at.left() + at.width() / 2, at.top() + 3));
		}

		if (showing_detail())
			draw_detail(inter);
		else
			draw_list(inter);
	}

	void UIQuestLog::draw_list(float inter) const
	{
		if (listed.empty())
		{
			rowtext.change_text(tab == AVAILABLE
				? "Nothing you can take right now."
				: (tab == IN_PROGRESS
					? "You are not on a quest."
					: "You have not finished a quest yet."));

			rowtext.draw(position + Point<int16_t>(PAD + 4, LIST_TOP + 6));

			return;
		}

		for (int16_t i = 0; i < rows_shown(); i++)
		{
			int16_t index = offset + i;

			if (index >= static_cast<int16_t>(listed.size()))
				break;

			const QuestData& data = QuestData::get(listed[index]);
			Rectangle<int16_t> at = row_bounds(i);

			GraphicsGL::get().drawrectangle(
				at.left(), at.top(), at.width(), at.height() - 2,
				1.0f, 1.0f, 1.0f, (i % 2) ? 0.05f : 0.09f);

			std::string name = data.is_valid()
				? data.get_name()
				: ("Quest " + std::to_string(listed[index]));

			// A level in front is what makes the Available list readable -
			// it is otherwise several hundred names in id order.
			int16_t lvmin = data.to_start().lvmin;

			if (tab == AVAILABLE && lvmin)
				name = "Lv" + std::to_string(lvmin) + "  " + name;

			rowtext.change_text(name);
			rowtext.draw(Point<int16_t>(at.left() + 6, at.top() + 3));
		}

		if (static_cast<int16_t>(listed.size()) > rows_shown())
			slider.draw(position);
	}

	void UIQuestLog::draw_detail(float inter) const
	{
		const Player& player = Stage::get().get_player();
		const QuestData& data = QuestData::get(opened);

		int16_t y = LIST_TOP;
		Point<int16_t> left = position + Point<int16_t>(PAD + 4, 0);

		heading.change_text(data.is_valid() ? data.get_name() : "Unknown quest");
		heading.draw(left + Point<int16_t>(0, y));
		y += 22;

		bool started = questlog.is_started(opened);
		bool done = questlog.is_completed(opened);

		// The journal entry for the state it is in: what to do, what is
		// being done, or how it ended.
		body.change_text(QuestData::strip_markup(
			data.get_text(done ? 2 : (started ? 1 : 0))));

		body.draw(left + Point<int16_t>(0, y));
		y += body.height() + 8;

		// What is still needed, with how far along it is. Only worth showing
		// while the quest is live - before and after it is noise.
		if (started)
		{
			const QuestData::Requirements& need = data.to_finish();

			if (!need.mobs.empty() || !need.items.empty())
			{
				label.change_text("STILL TO DO");
				label.draw(Point<int16_t>(position.x() + width() / 2, position.y() + y));
				y += 18;
			}

			size_t which = 0;

			for (const auto& mob : need.mobs)
			{
				int16_t have = questlog.killed(opened, which);

				rowtext.change_text(
					std::string(nl::nx::string["Mob.img"][std::to_string(mob.first)]["name"])
					+ "   " + std::to_string(have) + " / " + std::to_string(mob.second));

				rowtext.draw(left + Point<int16_t>(0, y));
				y += 16;
				which++;
			}

			for (const auto& item : need.items)
			{
				if (item.second <= 0)
					continue;

				int16_t have = player.get_inventory().get_total_item_count(item.first);

				rowtext.change_text(ItemData::get(item.first).get_name()
					+ "   " + std::to_string(have) + " / " + std::to_string(item.second));

				rowtext.draw(left + Point<int16_t>(0, y));
				y += 16;
			}

			y += 6;
		}

		// What it pays.
		const QuestData::Rewards& pays = data.finish_rewards();

		if (pays.exp || pays.money || !pays.items.empty())
		{
			label.change_text("REWARD");
			label.draw(Point<int16_t>(position.x() + width() / 2, position.y() + y));
			y += 18;

			std::string line;

			if (pays.exp)
				line += std::to_string(pays.exp) + " exp   ";

			if (pays.money)
				line += std::to_string(pays.money) + " mesos";

			if (!line.empty())
			{
				rowtext.change_text(line);
				rowtext.draw(left + Point<int16_t>(0, y));
				y += 16;
			}

			for (const auto& item : pays.items)
			{
				if (item.second <= 0)
					continue;

				rowtext.change_text(ItemData::get(item.first).get_name()
					+ " x" + std::to_string(item.second));

				rowtext.draw(left + Point<int16_t>(0, y));
				y += 16;
			}
		}

		// BACK always, GIVE UP only while it is live.
		Rectangle<int16_t> back = back_bounds();

		GraphicsGL::get().drawrectangle(
			back.left(), back.top(), back.width(), back.height(),
			0.16f, 0.17f, 0.22f, 0.95f);

		label.change_text("BACK");
		label.draw(Point<int16_t>(back.left() + back.width() / 2, back.top() + 3));

		if (started)
		{
			Rectangle<int16_t> act = action_bounds();

			GraphicsGL::get().drawrectangle(
				act.left(), act.top(), act.width(), act.height(),
				0.42f, 0.18f, 0.18f, 0.95f);

			label.change_text("GIVE UP");
			label.draw(Point<int16_t>(act.left() + act.width() / 2, act.top() + 3));
		}
	}

	Rectangle<int16_t> UIQuestLog::tab_bounds(Tab which) const
	{
		int16_t w = (width() - 2 * PAD) / TAB_COUNT;
		int16_t x = position.x() + PAD + which * w;

		return Rectangle<int16_t>(
			Point<int16_t>(x, position.y() + TAB_TOP),
			Point<int16_t>(x + w - 2, position.y() + TAB_TOP + TAB_H));
	}

	Rectangle<int16_t> UIQuestLog::row_bounds(int16_t row) const
	{
		int16_t y = position.y() + LIST_TOP + row * ROW_H;

		return Rectangle<int16_t>(
			Point<int16_t>(position.x() + PAD, y),
			Point<int16_t>(position.x() + width() - PAD - 12, y + ROW_H));
	}

	Rectangle<int16_t> UIQuestLog::back_bounds() const
	{
		constexpr int16_t W = 76;
		constexpr int16_t H = 20;

		Point<int16_t> at(position.x() + PAD, position.y() + height() - H - PAD);

		return Rectangle<int16_t>(at, at + Point<int16_t>(W, H));
	}

	Rectangle<int16_t> UIQuestLog::action_bounds() const
	{
		constexpr int16_t W = 88;
		constexpr int16_t H = 20;

		Point<int16_t> at(position.x() + width() - W - PAD,
			position.y() + height() - H - PAD);

		return Rectangle<int16_t>(at, at + Point<int16_t>(W, H));
	}

	void UIQuestLog::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (!pressed)
			return;

		if (escape)
		{
			// Escape steps back out of a quest before it closes the window.
			if (showing_detail())
				opened = 0;
			else
				deactivate();

			return;
		}

		if (keycode == KeyAction::Id::TAB)
		{
			tab = static_cast<Tab>((tab + 1) % TAB_COUNT);
			opened = 0;

			rebuild();
		}
	}

	void UIQuestLog::send_scroll(double yoffset)
	{
		if (slider.isenabled())
			slider.send_scroll(yoffset);
	}

	Cursor::State UIQuestLog::send_cursor(bool clicking, Point<int16_t> cursorpos)
	{
		if (!showing_detail() && static_cast<int16_t>(listed.size()) > rows_shown())
		{
			Cursor::State state = slider.send_cursor(cursorpos - position, clicking);

			if (state != Cursor::State::IDLE)
				return state;
		}

		if (clicking)
		{
			for (uint8_t t = 0; t < TAB_COUNT; t++)
			{
				if (!tab_bounds(static_cast<Tab>(t)).contains(cursorpos))
					continue;

				if (tab != t)
				{
					tab = static_cast<Tab>(t);
					opened = 0;

					rebuild();
					Sound(Sound::Name::TAB).play();
				}

				return Cursor::State::CANCLICK;
			}

			if (showing_detail())
			{
				if (back_bounds().contains(cursorpos))
				{
					opened = 0;

					return Cursor::State::CANCLICK;
				}

				if (questlog.is_started(opened) && action_bounds().contains(cursorpos))
				{
					// Giving up needs no NPC and no position.
					QuestActionPacket(opened).dispatch();

					opened = 0;

					return Cursor::State::CANCLICK;
				}
			}
			else
			{
				for (int16_t i = 0; i < rows_shown(); i++)
				{
					int16_t index = offset + i;

					if (index >= static_cast<int16_t>(listed.size()))
						break;

					if (!row_bounds(i).contains(cursorpos))
						continue;

					opened = listed[index];

					return Cursor::State::CANCLICK;
				}
			}
		}

		return UIDragElement::send_cursor(clicking, cursorpos);
	}

	bool UIQuestLog::indragrange(Point<int16_t> cursorpos) const
	{
		// Pinned when it is the panel's page - there is nothing to drag it to.
		if (panel)
			return false;

		return UIDragElement::indragrange(cursorpos);
	}

	UIElement::Type UIQuestLog::get_type() const
	{
		return TYPE;
	}
}
