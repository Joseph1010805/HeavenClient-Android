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

#include "../../Character/Inventory/Inventory.h"
#include "../../Graphics/Text.h"

#include <array>
#include <string>
#include <vector>

namespace ms
{
	// TWO PEOPLE PUTTING THINGS ON A TABLE.
	//
	// NO DRAGGING. The original trades by dragging out of the inventory
	// window into the trade window, which needs two windows on screen at once
	// and a cursor to carry something between them. This panel is 344 across
	// and is operated with a thumb.
	//
	// So the window carries its OWN strip of what you can trade, along the
	// bottom, and one tap moves an item from the strip to the table. That is
	// the whole interaction.
	//
	// ITEMS CANNOT COME BACK OFF. That is the server's rule, not a shortcut
	// here: v83 has no "take it back again" - the only way to undo a mistake
	// is to cancel the whole trade, which returns everything. The window says
	// so rather than letting somebody discover it.
	class UITrade : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::TRADE;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UITrade();

		void draw(float alpha) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursor_pos) override;

		UIElement::Type get_type() const override;

		// Laid out for the lower panel when there is one. Without this it
		// takes its natural size in the middle of the main screen, which is
		// what a one-screen device gets.
		void set_panel(Point<int16_t> room);

		// --- what the server tells us ---

		// A trade has opened. `slot` is which side of the table we are on -
		// the server calls it the number, 0 for whoever invited and 1 for
		// whoever accepted - and every item and meso message afterwards is
		// tagged with it, which is the only way to tell whose half moved.
		void opened(int8_t slot, std::string partner_name);

		// They walked in. Until this arrives the other half of the table is a
		// person who has not answered yet.
		void partner_joined(std::string partner_name);

		void put_item(int8_t whose, int8_t table_slot, int32_t item_id, int16_t count);
		void put_meso(int8_t whose, int32_t meso);

		// THEY have locked their half in. Ours is only locked when we say so.
		void partner_confirmed();

		void said(const std::string& line);

		// Over, one way or another. `operation` is the server's TradeResult:
		// 2 they cancelled, 7 done, 8 it failed, 9 one-of-a-kind limit,
		// 12 different maps, 13 damaged files.
		void closed(int8_t operation);

		bool is_open() const;

		// The trade is over AND its result has had its time on screen.
		//
		// The panel asks this: inside the panel this window is a PAGE, and a
		// page cannot simply deactivate itself - that would leave the second
		// screen blank with no way back. The panel turns back to whatever
		// page came before instead. On the main screen the window takes
		// itself away and nothing needs to ask.
		bool is_finished() const;

	private:
		// Nine slots a side, three across, which is what the server allows -
		// it refuses a target slot outside 1 to 9 and logs the sender as a
		// duper.
		static constexpr size_t TABLE = 9;
		static constexpr size_t COLS = 3;

		// How many of your own items the strip shows at once.
		static constexpr size_t STRIP = 7;

		struct Held
		{
			int32_t id = 0;
			int16_t count = 0;
		};

		// One line of the strip: where it lives in the bag, so it can be
		// named to the server, and what it is, so it can be drawn.
		struct Offer
		{
			InventoryType::Id type = InventoryType::Id::NONE;
			int16_t slot = 0;
			int32_t id = 0;
			int16_t count = 0;
		};

		void rebuild_offers();
		void place(const Offer& offer);
		void add_meso(int32_t amount);
		void confirm();
		void cancel();

		Rectangle<int16_t> table_cell(bool mine, size_t index) const;
		Rectangle<int16_t> strip_cell(size_t index) const;
		Rectangle<int16_t> strip_arrow(bool right) const;
		Rectangle<int16_t> meso_button(size_t index) const;
		Rectangle<int16_t> confirm_button() const;
		Rectangle<int16_t> cancel_button() const;

		void draw_cell(Rectangle<int16_t> box, int32_t item_id, int16_t count,
			bool lit) const;

		// Where the layout starts. The panel hands us a content box; on the
		// main screen we take a box of our own.
		Point<int16_t> room;

		// WHETHER SOMETHING IS ALREADY DRAWING A BACKGROUND BEHIND US.
		//
		// The second-screen panel paints its own parchment plate and this
		// window sits on it. The main screen paints nothing, so the same
		// white text landed straight on the map - unreadable on a bright
		// background, which is why a screen full of text read as "there is no
		// text". A device with no second screen, like the RP5, only ever
		// takes this path.
		bool in_panel = false;

		// Put the window back in the middle of whatever the screen is now.
		void recentre();

		// One side's offering, in words, at the given height.
		void draw_contents(bool is_mine, int16_t y) const;

		std::array<Held, TABLE> mine;
		std::array<Held, TABLE> theirs;

		int32_t my_meso = 0;
		int32_t their_meso = 0;

		// Our side of the table, as the server numbers it.
		int8_t my_number = 0;

		std::string partner;
		bool open = false;
		bool joined = false;
		bool i_confirmed = false;
		bool they_confirmed = false;

		// The next free slot on our half, counted from zero. The server wants
		// it from one; place() adds the one.
		int8_t next_free = 0;

		std::vector<Offer> offers;
		size_t strip_from = 0;

		// Refreshed on a beat rather than every frame - it walks five
		// inventories and the bag does not change between two frames.
		int16_t until_rebuild = 1;

		// The last thing worth saying: what went wrong, or what happened.
		std::string status;

		// HOW LONG THE RESULT STAYS UP AFTER A TRADE ENDS.
		//
		// "Trade complete." has to be readable - a result that vanishes in
		// the same frame the window closes is a result nobody sees - but it
		// must not sit on the screen for ever either, which is what it did:
		// the window stayed exactly as it was until something else moved it.
		// Counted in update() calls, which run about sixty times a second.
		static constexpr int16_t DISMISS_AFTER = 60 * 8;

		int16_t dismiss_in = 0;

		// Set when that countdown runs out. See is_finished().
		bool finished = false;

		mutable Text title;
		mutable Text label;
		mutable Text small;
		mutable Text count_text;
	};
}
