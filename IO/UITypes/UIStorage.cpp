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
#include "UIStorage.h"

#include "../UI.h"

#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/StoragePackets.h"

namespace ms
{
	namespace
	{
		// Measured to fit the lower panel's content box, which is about 296
		// by 234. Two lists, a tab strip, a money row and a way out all have
		// to live in that, which is why the cells are 26 and each list shows
		// two rows with arrows for the rest.
		constexpr int16_t CELL = 26;
		constexpr int16_t GAP = 3;

		constexpr int16_t TITLE_Y = 0;
		constexpr int16_t TAB_Y = 15;
		constexpr int16_t TAB_H = 18;

		constexpr int16_t BANK_LABEL_Y = 36;
		constexpr int16_t BANK_Y = 49;

		constexpr int16_t MESO_Y = BANK_Y + 2 * (CELL + GAP) + 2;
		constexpr int16_t MESO_BTN_H = 20;

		constexpr int16_t BAG_LABEL_Y = MESO_Y + MESO_BTN_H + 6;
		constexpr int16_t BAG_Y = BAG_LABEL_Y + 13;

		constexpr int16_t CLOSE_Y = BAG_Y + 2 * (CELL + GAP) + 4;
		constexpr int16_t CLOSE_H = 22;

		const char* TAB_NAMES[4] = { "EQUIP", "USE", "SET", "ETC" };

		const InventoryType::Id TAB_TYPES[4] =
		{
			InventoryType::Id::EQUIP,
			InventoryType::Id::USE,
			InventoryType::Id::SETUP,
			InventoryType::Id::ETC
		};

		constexpr int32_t MESO_STEPS[3] = { 1000, 10000, 100000 };

		std::string with_commas(int64_t value)
		{
			std::string digits = std::to_string(value < 0 ? -value : value);
			std::string out;

			int16_t since = 0;

			for (size_t i = digits.size(); i > 0; i--)
			{
				out.insert(out.begin(), digits[i - 1]);

				if (++since == 3 && i > 1)
				{
					out.insert(out.begin(), ',');
					since = 0;
				}
			}

			return (value < 0 ? "-" : "") + out;
		}
	}

	UIStorage::UIStorage() : UIElement(Point<int16_t>(0, 0), Point<int16_t>(300, 240))
	{
		title = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE);
		label = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
		small = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);
		count_text = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::WHITE);

		room = Point<int16_t>(300, 240);

		position = Point<int16_t>(
			static_cast<int16_t>((800 - room.x()) / 2),
			static_cast<int16_t>((600 - room.y()) / 2));
	}

	void UIStorage::set_panel(Point<int16_t> space)
	{
		room = space;
		dimension = space;
	}

	UIElement::Type UIStorage::get_type() const
	{
		return TYPE;
	}

	bool UIStorage::is_open() const
	{
		return open;
	}

	// --- what the server tells us ------------------------------------------

	void UIStorage::opened(int32_t npc_id, int8_t slots, int32_t meso,
		std::vector<Held> contents)
	{
		npc = npc_id;
		capacity = slots;
		bank_meso = meso;
		bank = std::move(contents);

		tab = 0;
		bank_from = 0;
		bag_from = 0;
		open = true;
		status.clear();
		until_rebuild = 1;
	}

	void UIStorage::replace_tab(InventoryType::Id type, std::vector<Held> contents)
	{
		// THE SERVER SENDS A WHOLE TAB, NOT THE ITEM THAT MOVED.
		//
		// So everything of that type is thrown away and replaced, and
		// everything else is left exactly where it was. Trying to work out
		// which single item changed would be guessing at something the packet
		// does not say.
		std::vector<Held> kept;

		for (const Held& held : bank)
			if (InventoryType::by_item_id(held.id) != type)
				kept.push_back(held);

		// The order within a tab is the order the server sent, and that order
		// IS the index it expects back - so the arriving list goes in whole,
		// not merged into the old one.
		for (const Held& held : contents)
			kept.push_back(held);

		bank = std::move(kept);

		if (bank_from >= bank.size())
			bank_from = 0;

		until_rebuild = 1;
	}

	void UIStorage::replace_all(std::vector<Held> contents)
	{
		bank = std::move(contents);

		bank_from = 0;
		until_rebuild = 1;
	}

	void UIStorage::set_meso(int32_t meso)
	{
		bank_meso = meso;
	}

	void UIStorage::refused(int8_t code)
	{
		switch (code)
		{
		case 0x0A:
			status = "Your bag is full.";
			break;
		case 0x0B:
			status = "Not enough meso for the fee.";
			break;
		case 0x0C:
			status = "You already have one of those.";
			break;
		default:
			status = "The storage keeper refused that.";
			break;
		}
	}

	void UIStorage::closed()
	{
		open = false;
		bank.clear();
		bag.clear();
	}

	// --- layout ------------------------------------------------------------

	InventoryType::Id UIStorage::tab_type() const
	{
		return TAB_TYPES[tab < TABS ? tab : 0];
	}

	std::vector<size_t> UIStorage::tab_view() const
	{
		// INDEXES INTO THE WHOLE LIST, NOT COPIES.
		//
		// What the server wants back when taking something out is the item's
		// place WITHIN ITS TAB, counted from zero. Keeping the positions
		// rather than the items means that number is simply where it sits in
		// here, and there is no second numbering to keep in step.
		std::vector<size_t> out;

		InventoryType::Id want = tab_type();

		for (size_t i = 0; i < bank.size(); i++)
			if (InventoryType::by_item_id(bank[i].id) == want)
				out.push_back(i);

		return out;
	}

	Rectangle<int16_t> UIStorage::tab_button(size_t which) const
	{
		int16_t w = static_cast<int16_t>((room.x() - 20 - 3 * 6) / TABS);

		Point<int16_t> at = position + Point<int16_t>(
			static_cast<int16_t>(10 + which * (w + 6)), TAB_Y);

		return Rectangle<int16_t>(at, at + Point<int16_t>(w, TAB_H));
	}

	Rectangle<int16_t> UIStorage::bank_cell(size_t index) const
	{
		int16_t used = static_cast<int16_t>(COLS * CELL + (COLS - 1) * GAP);
		int16_t left = static_cast<int16_t>((room.x() - used) / 2);

		Point<int16_t> at = position + Point<int16_t>(
			static_cast<int16_t>(left + (index % COLS) * (CELL + GAP)),
			static_cast<int16_t>(BANK_Y + (index / COLS) * (CELL + GAP)));

		return Rectangle<int16_t>(at, at + Point<int16_t>(CELL, CELL));
	}

	Rectangle<int16_t> UIStorage::bag_cell(size_t index) const
	{
		int16_t used = static_cast<int16_t>(COLS * CELL + (COLS - 1) * GAP);
		int16_t left = static_cast<int16_t>((room.x() - used) / 2);

		Point<int16_t> at = position + Point<int16_t>(
			static_cast<int16_t>(left + (index % COLS) * (CELL + GAP)),
			static_cast<int16_t>(BAG_Y + (index / COLS) * (CELL + GAP)));

		return Rectangle<int16_t>(at, at + Point<int16_t>(CELL, CELL));
	}

	Rectangle<int16_t> UIStorage::meso_button(size_t which) const
	{
		// Six: take a thousand, ten thousand, a hundred thousand OUT, and the
		// same three IN. Which way round is said on the button, because
		// "+1k" on its own does not say whose thousand it is.
		constexpr size_t COUNT = 6;

		int16_t w = static_cast<int16_t>((room.x() - 20 - 5 * 4) / COUNT);

		Point<int16_t> at = position + Point<int16_t>(
			static_cast<int16_t>(10 + which * (w + 4)),
			static_cast<int16_t>(MESO_Y + 1));

		return Rectangle<int16_t>(at, at + Point<int16_t>(w, MESO_BTN_H));
	}

	Rectangle<int16_t> UIStorage::close_button() const
	{
		Point<int16_t> at = position + Point<int16_t>(10, CLOSE_Y);

		return Rectangle<int16_t>(at, at + Point<int16_t>(
			static_cast<int16_t>(room.x() - 20), CLOSE_H));
	}

	// --- drawing -----------------------------------------------------------

	void UIStorage::draw_cell(Rectangle<int16_t> box, int32_t item_id,
		int16_t count) const
	{
		GraphicsGL::get().drawrectangle(
			box.left(), box.top(), box.width(), box.height(),
			item_id ? 0.16f : 0.10f,
			item_id ? 0.19f : 0.11f,
			item_id ? 0.23f : 0.13f, 0.90f);

		if (!item_id)
			return;

		const Texture& icon = ItemData::get(item_id).get_icon(false);

		if (icon.is_valid())
		{
			Point<int16_t> size = icon.get_dimensions();

			double fit = size.x() > 0
				? static_cast<double>(box.width() - 4) / size.x()
				: 1.0;

			if (fit > 1.0)
				fit = 1.0;

			Point<int16_t> to(
				static_cast<int16_t>(size.x() * fit),
				static_cast<int16_t>(size.y() * fit));

			icon.draw(DrawArgument(Point<int16_t>(
				static_cast<int16_t>(box.left() + (box.width() - to.x()) / 2),
				static_cast<int16_t>(box.top() + (box.height() - to.y()) / 2)), to));
		}

		if (count > 1)
		{
			count_text.change_text(std::to_string(count));
			count_text.draw(Point<int16_t>(
				static_cast<int16_t>(box.right() - 2),
				static_cast<int16_t>(box.bottom() - 15)));
		}
	}

	void UIStorage::draw(float alpha) const
	{
		if (!open)
		{
			label.change_text("The bank is closed.");
			label.draw(position + Point<int16_t>(
				static_cast<int16_t>(room.x() / 2),
				static_cast<int16_t>(room.y() / 2 - 20)));

			small.change_text("Talk to a storage keeper to open it.");
			small.draw(position + Point<int16_t>(10,
				static_cast<int16_t>(room.y() / 2 + 4)));

			return;
		}

		title.change_text("STORAGE  -  " + std::to_string(bank.size())
			+ " of " + std::to_string(static_cast<int32_t>(capacity)));

		title.draw(position + Point<int16_t>(
			static_cast<int16_t>(room.x() / 2), TITLE_Y));

		for (size_t i = 0; i < TABS; i++)
		{
			Rectangle<int16_t> box = tab_button(i);
			bool here = (i == tab);

			GraphicsGL::get().drawrectangle(
				box.left(), box.top(), box.width(), box.height(),
				here ? 0.16f : 0.11f,
				here ? 0.30f : 0.12f,
				here ? 0.20f : 0.15f, 1.0f);

			label.change_text(TAB_NAMES[i]);
			label.draw(Point<int16_t>(
				box.left() + box.width() / 2, box.top() + 2));
		}

		std::vector<size_t> shown = tab_view();

		small.change_text("IN THE BANK - tap to take out");
		small.draw(position + Point<int16_t>(10, BANK_LABEL_Y));

		for (size_t i = 0; i < PAGE; i++)
		{
			size_t at = bank_from + i;

			if (at < shown.size())
				draw_cell(bank_cell(i), bank[shown[at]].id, bank[shown[at]].count);
			else
				draw_cell(bank_cell(i), 0, 0);
		}

		// MONEY, AND WHICH WAY IT IS GOING.
		small.change_text("Bank: " + with_commas(bank_meso) + " meso");
		small.draw(position + Point<int16_t>(10, static_cast<int16_t>(MESO_Y - 13)));

		for (size_t i = 0; i < 6; i++)
		{
			Rectangle<int16_t> box = meso_button(i);

			GraphicsGL::get().drawrectangle(
				box.left(), box.top(), box.width(), box.height(),
				0.17f, 0.19f, 0.23f, 1.0f);

			// The first three take money OUT of the bank, the last three put
			// it in - which is the same order the server reads the sign in.
			std::string thousands = std::to_string(MESO_STEPS[i % 3] / 1000) + "k";

			label.change_text((i < 3 ? "out " : "in ") + thousands);
			label.draw(Point<int16_t>(
				box.left() + box.width() / 2, box.top() + 2));
		}

		small.change_text("IN YOUR BAG - tap to put away");
		small.draw(position + Point<int16_t>(10, BAG_LABEL_Y));

		for (size_t i = 0; i < PAGE; i++)
		{
			size_t at = bag_from + i;

			if (at < bag.size())
				draw_cell(bag_cell(i), bag[at].id, bag[at].count);
			else
				draw_cell(bag_cell(i), 0, 0);
		}

		Rectangle<int16_t> out = close_button();

		GraphicsGL::get().drawrectangle(
			out.left(), out.top(), out.width(), out.height(),
			0.30f, 0.13f, 0.13f, 1.0f);

		label.change_text(status.empty() ? "CLOSE" : status);
		label.draw(Point<int16_t>(
			out.left() + out.width() / 2, out.top() + 3));
	}

	// --- what it does ------------------------------------------------------

	void UIStorage::rebuild_bag()
	{
		bag.clear();

		if (!Stage::get().is_active())
			return;

		const Inventory& carried = Stage::get().get_player().get_inventory();

		InventoryType::Id type = tab_type();

		int16_t last = static_cast<int16_t>(carried.get_slotmax(type));

		for (int16_t slot = 1; slot <= last; slot++)
		{
			int32_t id = carried.get_item_id(type, slot);

			if (!id)
				continue;

			Bagged one;

			one.slot = slot;
			one.id = id;
			one.count = carried.get_item_count(type, slot);

			bag.push_back(one);
		}

		if (bag_from >= bag.size())
			bag_from = 0;
	}

	void UIStorage::take_out(size_t index)
	{
		std::vector<size_t> shown = tab_view();

		if (index >= shown.size())
			return;

		// The index WITHIN THE TAB, counted from zero - see StoragePackets.h.
		StorageTakeOutPacket(tab_type(), static_cast<int8_t>(index)).dispatch();

		status.clear();
	}

	void UIStorage::store(size_t index)
	{
		if (index >= bag.size())
			return;

		const Bagged& one = bag[index];

		// The whole stack, and the REAL bag slot counted from one. Anything
		// below one is treated by the server as a packet edit and it
		// disconnects rather than complaining.
		StorageStorePacket(one.slot, one.id,
			one.count > 0 ? one.count : 1).dispatch();

		status.clear();
	}

	void UIStorage::move_meso(int32_t amount)
	{
		// Positive takes it out of the bank, negative puts it in. Never more
		// than there is at the end it is coming from - the server would
		// simply refuse and say nothing useful.
		int64_t held = Stage::get().is_active()
			? Stage::get().get_player().get_inventory().get_meso()
			: 0;

		if (amount > 0 && amount > bank_meso)
			amount = bank_meso;

		if (amount < 0 && -static_cast<int64_t>(amount) > held)
			amount = static_cast<int32_t>(-held);

		if (amount == 0)
			return;

		StorageMesoPacket(amount).dispatch();

		status.clear();
	}

	void UIStorage::update()
	{
		UIElement::update();

		if (!open)
			return;

		if (--until_rebuild > 0)
			return;

		until_rebuild = 30;

		rebuild_bag();
	}

	Cursor::State UIStorage::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (!open)
			return UIElement::send_cursor(clicked, cursorpos);

		if (close_button().contains(cursorpos))
		{
			if (clicked)
			{
				StorageClosePacket().dispatch();

				closed();
			}

			return Cursor::State::CANCLICK;
		}

		for (size_t i = 0; i < TABS; i++)
		{
			if (!tab_button(i).contains(cursorpos))
				continue;

			if (clicked && i != tab)
			{
				tab = i;
				bank_from = 0;
				bag_from = 0;
				until_rebuild = 1;

				rebuild_bag();
			}

			return Cursor::State::CANCLICK;
		}

		for (size_t i = 0; i < 6; i++)
		{
			if (!meso_button(i).contains(cursorpos))
				continue;

			if (clicked)
				move_meso(i < 3 ? MESO_STEPS[i] : -MESO_STEPS[i - 3]);

			return Cursor::State::CANCLICK;
		}

		for (size_t i = 0; i < PAGE; i++)
		{
			if (bank_cell(i).contains(cursorpos))
			{
				if (clicked)
					take_out(bank_from + i);

				return Cursor::State::CANCLICK;
			}

			if (bag_cell(i).contains(cursorpos))
			{
				if (clicked)
					store(bag_from + i);

				return Cursor::State::CANCLICK;
			}
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}
}
