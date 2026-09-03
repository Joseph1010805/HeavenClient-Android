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
#include "UICharacterPage.h"

namespace ms
{
	UICharacterPage::UICharacterPage(const CharStats& stats, const Skillbook& skills)
		: UIElement(Point<int16_t>(0, 0), Point<int16_t>(0, 0))
	{
		stats_window = std::make_unique<UIStatsinfo>(stats);
		skill_window = std::make_unique<UISkillbook>(stats, skills);

		divider_label = Text(Text::Font::A13B, Text::Alignment::LEFT,
			Color::Name::WHITE, "SKILLS");
	}

	void UICharacterPage::set_panel(Point<int16_t> screen)
	{
		panel = screen;

		stats_window->set_panel(screen);
		skill_window->set_panel(screen);

		dimension = Point<int16_t>(screen.x(), screen.y());
	}

	Point<int16_t> UICharacterPage::stats_at() const
	{
		Point<int16_t> size = stats_window->get_dimension();

		return Point<int16_t>((panel.x() - size.x()) / 2, scroll);
	}

	Point<int16_t> UICharacterPage::skills_at() const
	{
		Point<int16_t> size = skill_window->get_dimension();
		int16_t below = stats_window->get_dimension().y() + GAP + LABEL_H;

		return Point<int16_t>((panel.x() - size.x()) / 2,
			static_cast<int16_t>(scroll + below));
	}

	int16_t UICharacterPage::column_height() const
	{
		// PLUS A TAIL.
		//
		// The column stopped at the last pixel either window claimed, which
		// put the skill page's SPEND row hard against the bottom edge of the
		// panel - underneath the EXP gauge and the TO HOTKEYS button, both of
		// which are drawn after the page. Scrolling could reach it and still
		// not show it. The tail is the room those two need.
		constexpr int16_t TAIL = 56;

		return static_cast<int16_t>(
			stats_window->get_dimension().y() + GAP + LABEL_H
			+ skill_window->get_dimension().y() + TAIL);
	}

	void UICharacterPage::update()
	{
		UIElement::update();

		stats_window->update();
		skill_window->update();
	}

	void UICharacterPage::draw(float inter) const
	{
		// Positioned every frame rather than once: either half can change
		// height as skills are learnt or the stat list expands, and a stale
		// position would leave the two overlapping.
		stats_window->set_position(stats_at());
		skill_window->set_position(skills_at());

		stats_window->draw(inter);

		// Inset, or the gauge down the left edge takes the S off "SKILLS".
		Point<int16_t> at = skills_at();
		divider_label.draw(Point<int16_t>(at.x() + 16, at.y() - LABEL_H));

		skill_window->draw(inter);
	}

	void UICharacterPage::send_scroll(double yoffset)
	{
		// The column moves; the windows keep their own sliders. A skill list
		// long enough to need one still scrolls itself once this reaches the
		// bottom.
		int16_t room = static_cast<int16_t>(column_height() - panel.y());

		if (room <= 0)
		{
			scroll = 0;
			return;
		}

		scroll = static_cast<int16_t>(scroll + yoffset * 32);

		if (scroll > 0)
			scroll = 0;
		else if (scroll < -room)
			scroll = static_cast<int16_t>(-room);
	}

	Cursor::State UICharacterPage::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// Both halves are positioned in the panel's own coordinates, so the
		// cursor needs no adjusting - only sending to the right one.
		stats_window->set_position(stats_at());
		skill_window->set_position(skills_at());

		Point<int16_t> at = skills_at();

		if (cursorpos.y() >= at.y())
			return skill_window->send_cursor(clicked, cursorpos);

		return stats_window->send_cursor(clicked, cursorpos);
	}

	Keyboard::Mapping UICharacterPage::selected_mapping() const
	{
		return skill_window->selected_mapping();
	}

	UIElement::Type UICharacterPage::get_type() const
	{
		return TYPE;
	}
}
