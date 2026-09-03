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

#include "../../Character/Inventory/InventoryType.h"
#include "../../Graphics/Text.h"

#include <string>
#include <vector>

namespace ms
{
	// THE ACCOUNT'S BANK.
	//
	// One store shared by every character on the account, which is what makes
	// it worth having here: it is how a character hands something to another
	// character of the same person's without a second device and a trade.
	//
	// TWO LISTS, NOT A GRID. The original is a 4-tab window you drag between,
	// which needs two windows on screen and a cursor. This shows what is in
	// the bank on top and what is in the bag underneath, and a tap moves one
	// item between them - the same one-tap idea the trade table uses.
	//
	// The tabs are ours, not the server's. Storage arrives as ONE flat list
	// and the server works out an item's type from its id; the tab is only a
	// way of not showing a hundred things at once, and the index sent back is
	// the item's place within its own tab.
	class UIStorage : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::STORAGE;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UIStorage();

		void draw(float alpha) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursor_pos) override;

		UIElement::Type get_type() const override;

		void set_panel(Point<int16_t> room);

		// --- what the server tells us ---

		struct Held
		{
			int32_t id = 0;
			int16_t count = 0;
		};

		// The bank has been opened by an NPC. There is no request that causes
		// this - it simply arrives.
		void opened(int32_t npc_id, int8_t slots, int32_t meso,
			std::vector<Held> contents);

		// The whole of one tab, after something went in or came out. The
		// server sends the tab that changed, not the item that moved.
		void replace_tab(InventoryType::Id type, std::vector<Held> contents);

		// Everything, renumbered. Only arrives after a tidy-up, which
		// reorders the whole bank rather than one tab.
		void replace_all(std::vector<Held> contents);

		void set_meso(int32_t meso);

		// 0x0A no room in the bag, 0x0B not enough meso, 0x0C one of a kind.
		void refused(int8_t code);

		void closed();

		bool is_open() const;

	private:
		static constexpr size_t TABS = 4;

		// TWO ROWS EACH, EIGHT ACROSS.
		//
		// The panel's content box is about 296 by 234 and it has to hold two
		// lists, a tab strip, a money row and a way out. Two rows apiece is
		// what is left over; the rest of a full bank is reached by changing
		// tab, which is also how the server thinks about it.
		static constexpr size_t ROWS = 2;
		static constexpr size_t COLS = 8;
		static constexpr size_t PAGE = ROWS * COLS;

		void take_out(size_t index);
		void store(size_t index);
		void move_meso(int32_t amount);
		void rebuild_bag();

		// What is in the bank under the tab being shown, in the order the
		// server sent it - which IS the index it wants back.
		std::vector<size_t> tab_view() const;

		InventoryType::Id tab_type() const;

		Rectangle<int16_t> tab_button(size_t which) const;
		Rectangle<int16_t> bank_cell(size_t index) const;
		Rectangle<int16_t> bag_cell(size_t index) const;
		Rectangle<int16_t> meso_button(size_t which) const;
		Rectangle<int16_t> close_button() const;

		void draw_cell(Rectangle<int16_t> box, int32_t item_id, int16_t count) const;

		Point<int16_t> room;

		// Everything in the bank, every tab, exactly as it arrived.
		std::vector<Held> bank;

		// What is in the bag under the tab being shown, with the slot it
		// lives in - the server wants the real bag slot, counted from one.
		struct Bagged
		{
			int16_t slot = 0;
			int32_t id = 0;
			int16_t count = 0;
		};

		std::vector<Bagged> bag;

		int32_t npc = 0;
		int32_t bank_meso = 0;
		int8_t capacity = 0;

		size_t tab = 0;
		size_t bank_from = 0;
		size_t bag_from = 0;

		bool open = false;

		std::string status;

		int16_t until_rebuild = 1;

		mutable Text title;
		mutable Text label;
		mutable Text small;
		mutable Text count_text;
	};
}
