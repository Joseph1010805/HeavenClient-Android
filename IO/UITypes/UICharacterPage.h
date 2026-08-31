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

#include "UIStatsinfo.h"
#include "UISkillbook.h"

#include "../UIElement.h"

#include "../../Graphics/Text.h"

namespace ms
{
	// The Character page: your stats, with your skills underneath.
	//
	// Two of the game's own windows in ONE scrollable column, so Skills stops
	// being a page of its own. They are built and owned here rather than
	// borrowed, because each has to be told where to sit every frame - which
	// is not something a window shared with the main screen can have done to
	// it from two places at once.
	//
	// Scrolling moves the column, not the windows: each keeps whatever slider
	// it already had, and this slides both past the viewport together.
	class UICharacterPage : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::CHARACTERPAGE;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UICharacterPage(const CharStats& stats, const Skillbook& skills);

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_scroll(double yoffset) override;

		// Whatever the skill half has picked out, so a skill can be carried to
		// the hotkey page exactly as it could when Skills was its own page.
		Keyboard::Mapping selected_mapping() const override;

		UIElement::Type get_type() const override;

		void set_panel(Point<int16_t> screen);

	private:
		// Where each half sits inside the column, before scrolling.
		Point<int16_t> stats_at() const;
		Point<int16_t> skills_at() const;

		int16_t column_height() const;

		std::unique_ptr<UIStatsinfo> stats_window;
		std::unique_ptr<UISkillbook> skill_window;

		Point<int16_t> panel;

		// How far down the column we are. Never above zero - the top of the
		// stats is the top of the page.
		int16_t scroll = 0;

		// Gap between the two halves, and the height of the label that names
		// the lower one.
		static constexpr int16_t GAP = 14;
		static constexpr int16_t LABEL_H = 22;

		mutable Text divider_label;
	};
}
