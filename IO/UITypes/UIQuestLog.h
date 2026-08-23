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

#include "../Character/Questlog.h"
#include "../Components/Slider.h"
#include "../Graphics/Text.h"

namespace ms
{
	// The quest journal.
	//
	// Drawn by hand rather than stamped from `UIWindow2.img/Quest`, for the
	// same reason the cash shop is: that artwork is one picture with its list
	// area, its search bar and its notice strip already painted in, so
	// nothing can be moved and there is nowhere to put anything that was not
	// in the original. What was here before drew the picture, three tabs, a
	// search box and a scroll bar, and listed no quests at all.
	//
	// One column, because this has to be readable on a phone: the list, and
	// tapping a row swaps to that quest's page with a way back. Side by side
	// would give each half 170 pixels.
	class UIQuestLog : public UIDragElement<PosQUEST>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::QUESTLOG;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIQuestLog(const Questlog& questLog);

		// Show this copy on the lower panel rather than over the game.
		void set_panel(Point<int16_t> screen);

		void draw(float inter) const override;
		void update() override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;
		Cursor::State send_cursor(bool clicking, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

	protected:
		bool indragrange(Point<int16_t> cursorpos) const override;

	private:
		enum Tab : uint8_t
		{
			AVAILABLE,
			IN_PROGRESS,
			COMPLETED,
			TAB_COUNT
		};

		static const char* tab_name(Tab which);

		// Which quests the open tab holds. Rebuilt when the tab changes and
		// when the log changes underneath it.
		void rebuild();

		// The chosen quest's page, or the list. A phone has no room for both.
		bool showing_detail() const;

		void draw_plate(Point<int16_t> at, int16_t w, int16_t h) const;
		void draw_list(float inter) const;
		void draw_detail(float inter) const;

		// Where the rows and the tabs are, for hit-testing.
		Rectangle<int16_t> tab_bounds(Tab which) const;
		Rectangle<int16_t> row_bounds(int16_t row) const;
		Rectangle<int16_t> back_bounds() const;
		Rectangle<int16_t> action_bounds() const;

		int16_t width() const;
		int16_t height() const;
		int16_t rows_shown() const;

		static constexpr int16_t WINDOW_W = 320;
		static constexpr int16_t WINDOW_H = 340;

		static constexpr int16_t PAD = 8;
		static constexpr int16_t TAB_TOP = 24;
		static constexpr int16_t TAB_H = 24;
		static constexpr int16_t LIST_TOP = TAB_TOP + TAB_H + 6;
		static constexpr int16_t ROW_H = 24;

		const Questlog& questlog;

		Tab tab = IN_PROGRESS;
		std::vector<int16_t> listed;
		int16_t offset = 0;

		// Zero means the list is showing.
		int16_t opened = 0;

		// So the list notices when a quest is taken or handed in without the
		// player touching this window.
		size_t last_started = 0;
		size_t last_completed = 0;

		bool panel = false;
		Point<int16_t> panel_screen;

		Slider slider;

		mutable Text heading;
		mutable Text rowtext;
		mutable Text body;
		mutable Text label;
	};
}
