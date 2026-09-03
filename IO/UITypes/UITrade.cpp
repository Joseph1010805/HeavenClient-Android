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
#include "UITrade.h"

#include "../UI.h"

#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/TradePackets.h"

namespace ms
{
	namespace
	{
		// THE SHAPE OF THE TABLE, in the panel's own units.
		//
		// Everything is measured from the top left of whatever box we are
		// given, so the same numbers work on the panel and on the main
		// screen. They are tight: the panel's content box is about 296 by 234
		// and all of this has to live inside it, which is why the cells are
		// 26 rather than the 32 the game uses.
		// 22, NOT 26.
		//
		// Four pixels off each cell is twelve off the grid and twelve more
		// off the strip, and that is what pays for the two lines at the
		// bottom naming what is actually on the table. An icon at 22 is still
		// a clear picture; a trade where you cannot read what you are being
		// given is not a trade anybody should agree to.
		constexpr int16_t CELL = 22;
		constexpr int16_t CELL_GAP = 3;

		constexpr int16_t GRID_W = CELL * 3 + CELL_GAP * 2;
		constexpr int16_t GRID_GAP = 16;

		constexpr int16_t TITLE_Y = 0;
		constexpr int16_t HEAD_Y = 16;

		// SIX MORE THAN THE TEXT NEEDS, not two.
		//
		// "YOU" and "THEM" were half-buried under the top row of cells: the
		// grid is drawn after the labels, so any overlap eats them. The row
		// has to clear the label's DESCENDERS, not just its baseline.
		//
		// The panel's content box is about 234 tall and this layout already
		// filled 230 of it, so the six is BOUGHT back below by tightening the
		// two eight-pixel gaps to five. The bottom of the window stays put.
		constexpr int16_t GRID_Y = 38;
		constexpr int16_t MESO_Y = GRID_Y + GRID_W + 4;
		constexpr int16_t MESO_BTN_Y = MESO_Y + 18;
		constexpr int16_t MESO_BTN_H = 22;
		constexpr int16_t STRIP_Y = MESO_BTN_Y + MESO_BTN_H + 3;
		constexpr int16_t BUTTON_Y = STRIP_Y + CELL + 3;
		constexpr int16_t BUTTON_H = 24;

		constexpr int16_t ARROW_W = 20;

		// The four amounts a thumb can add. Meso in this game arrives in
		// round numbers and the alternative - a number pad for a field that
		// is nearly always a multiple of a thousand - is more presses for
		// less certainty.
		constexpr int32_t MESO_STEPS[3] = { 1000, 10000, 100000 };

		std::string with_commas(int64_t value)
		{
			std::string digits = std::to_string(value);
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

			return out;
		}
	}

	UITrade::UITrade() : UIElement(Point<int16_t>(0, 0), Point<int16_t>(300, 240))
	{
		title = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE);
		label = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
		small = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);
		count_text = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::WHITE);

		// ROOMIER THAN THE PANEL'S BOX. On the main screen there is no reason
		// to be confined to the second screen's 300x240 - the extra height is
		// what the list of item NAMES goes in, below the grids.
		room = Point<int16_t>(360, 300);

		recentre();
	}

	// THE MIDDLE OF THE SCREEN WE ACTUALLY HAVE.
	//
	// This used to divide 800 and 600 - numbers that are not the view size and
	// have not been since the default became 1280x720. The window was pinned
	// well up and left of centre on every device, which is most of what "it
	// doesn't align" meant.
	void UITrade::recentre()
	{
		if (in_panel)
			return;

		int16_t width = Constants::Constants::get().get_viewwidth();
		int16_t height = Constants::Constants::get().get_viewheight();

		position = Point<int16_t>(
			static_cast<int16_t>((width - room.x()) / 2),
			static_cast<int16_t>((height - room.y()) / 2));
	}

	void UITrade::set_panel(Point<int16_t> space)
	{
		room = space;
		dimension = space;
		in_panel = true;
	}

	UIElement::Type UITrade::get_type() const
	{
		return TYPE;
	}

	bool UITrade::is_open() const
	{
		return open;
	}

	bool UITrade::is_finished() const
	{
		return finished;
	}

	void UITrade::opened(int8_t slot, std::string partner_name)
	{
		// The view size can change after start-up, and a window pinned
		// where the screen used to be is the same fault as the hardcoded
		// 800x600 this replaced.
		recentre();

		// A new trade cancels any pending dismissal of the last one's result.
		dismiss_in = 0;
		finished = false;
		makeactive();

		mine.fill(Held());
		theirs.fill(Held());

		my_meso = 0;
		their_meso = 0;
		next_free = 0;

		i_confirmed = false;
		they_confirmed = false;

		my_number = slot;
		partner = std::move(partner_name);

		// Being handed a partner's name at this point means we ACCEPTED an
		// invitation - the server sends the inviter an empty table and fills
		// the other half in later, when somebody says yes.
		joined = !partner.empty();
		open = true;

		status = joined
			? std::string()
			: "Waiting for them to answer...";

		until_rebuild = 1;
		strip_from = 0;
	}

	void UITrade::partner_joined(std::string partner_name)
	{
		partner = std::move(partner_name);
		joined = true;
		status.clear();
	}

	void UITrade::put_item(int8_t whose, int8_t table_slot, int32_t item_id, int16_t count)
	{
		// The server counts the table from one and we count from zero.
		size_t index = static_cast<size_t>(table_slot) - 1;

		if (table_slot < 1 || index >= TABLE)
			return;

		auto& side = (whose == my_number) ? mine : theirs;

		side[index].id = item_id;
		side[index].count = count;

		// Trust the server about where our own things landed. Placing is
		// optimistic - the strip is tapped and the packet goes - and the
		// server can refuse an item for reasons this side does not know
		// about, so this is what actually decides the next free slot.
		if (whose == my_number && static_cast<int8_t>(index) >= next_free)
			next_free = static_cast<int8_t>(index) + 1;

		until_rebuild = 1;
	}

	void UITrade::put_meso(int8_t whose, int32_t meso)
	{
		if (whose == my_number)
			my_meso = meso;
		else
			their_meso = meso;
	}

	void UITrade::partner_confirmed()
	{
		they_confirmed = true;

		status = partner + " is ready.";
	}

	void UITrade::said(const std::string& line)
	{
		status = line;
	}

	void UITrade::closed(int8_t operation)
	{
		open = false;
		dismiss_in = DISMISS_AFTER;
		joined = false;
		i_confirmed = false;
		they_confirmed = false;

		// The server's own numbering - see Trade.TradeResult. Said in words,
		// because "trade result 9" tells a player nothing about what to do
		// differently.
		switch (operation)
		{
		// 1 IS NOT A FAILURE, OR NOT ONLY ONE.
		//
		// Cosmic's Trade.completeTrade sends SUCCESSFUL(7) for a trade in
		// which you received no meso, and NO_RESPONSE(1) for one in which you
		// did - the money arrives with its own "Transaction completed" notice
		// and the result byte is reused. So a trade that worked perfectly
		// reported "the trade did not go through" to whichever side was paid,
		// while the other side was told it completed. Both were right about
		// the trade and one was lied to about it.
		//
		// The same 1 also means "you are the one who cancelled" (see
		// Trade.tradeResultsPair). We can tell the two apart because we know
		// what was on their half of the table: money coming to us is the only
		// reason a COMPLETED trade ends this way.
		case 1:
			status = (their_meso > 0)
				? "Trade complete."
				: "Trade closed.";
			break;
		case 2:
			status = "They cancelled the trade.";
			break;
		case 7:
			status = "Trade complete.";
			break;
		case 9:
			status = "One of you cannot hold two of a one-of-a-kind item.";
			break;
		case 12:
			status = "You have to be on the same map.";
			break;
		case 13:
			status = "The trade failed. Game files damaged.";
			break;
		default:
			status = "The trade did not go through.";
			break;
		}
	}

	// --- layout ------------------------------------------------------------

	Rectangle<int16_t> UITrade::table_cell(bool is_mine, size_t index) const
	{
		int16_t both = static_cast<int16_t>(GRID_W * 2 + GRID_GAP);
		int16_t left = static_cast<int16_t>((room.x() - both) / 2);

		if (!is_mine)
			left = static_cast<int16_t>(left + GRID_W + GRID_GAP);

		int16_t col = static_cast<int16_t>(index % COLS);
		int16_t row = static_cast<int16_t>(index / COLS);

		Point<int16_t> at = position + Point<int16_t>(
			static_cast<int16_t>(left + col * (CELL + CELL_GAP)),
			static_cast<int16_t>(GRID_Y + row * (CELL + CELL_GAP)));

		return Rectangle<int16_t>(at, at + Point<int16_t>(CELL, CELL));
	}

	Rectangle<int16_t> UITrade::strip_cell(size_t index) const
	{
		int16_t used = static_cast<int16_t>(STRIP * CELL + (STRIP - 1) * CELL_GAP);
		int16_t left = static_cast<int16_t>((room.x() - used) / 2);

		Point<int16_t> at = position + Point<int16_t>(
			static_cast<int16_t>(left + index * (CELL + CELL_GAP)), STRIP_Y);

		return Rectangle<int16_t>(at, at + Point<int16_t>(CELL, CELL));
	}

	Rectangle<int16_t> UITrade::strip_arrow(bool right) const
	{
		int16_t used = static_cast<int16_t>(STRIP * CELL + (STRIP - 1) * CELL_GAP);
		int16_t left = static_cast<int16_t>((room.x() - used) / 2);

		int16_t x = right
			? static_cast<int16_t>(left + used + 4)
			: static_cast<int16_t>(left - ARROW_W - 4);

		Point<int16_t> at = position + Point<int16_t>(x, STRIP_Y);

		return Rectangle<int16_t>(at, at + Point<int16_t>(ARROW_W, CELL));
	}

	Rectangle<int16_t> UITrade::meso_button(size_t index) const
	{
		// Three amounts and a clear, spread across the width.
		constexpr size_t COUNT = 4;

		int16_t w = static_cast<int16_t>((room.x() - 20 - 3 * 6) / COUNT);

		Point<int16_t> at = position + Point<int16_t>(
			static_cast<int16_t>(10 + index * (w + 6)), MESO_BTN_Y);

		return Rectangle<int16_t>(at, at + Point<int16_t>(w, MESO_BTN_H));
	}

	Rectangle<int16_t> UITrade::confirm_button() const
	{
		int16_t w = static_cast<int16_t>((room.x() - 20 - 8) / 2);

		Point<int16_t> at = position + Point<int16_t>(10, BUTTON_Y);

		return Rectangle<int16_t>(at, at + Point<int16_t>(w, BUTTON_H));
	}

	Rectangle<int16_t> UITrade::cancel_button() const
	{
		int16_t w = static_cast<int16_t>((room.x() - 20 - 8) / 2);

		Point<int16_t> at = position + Point<int16_t>(
			static_cast<int16_t>(10 + w + 8), BUTTON_Y);

		return Rectangle<int16_t>(at, at + Point<int16_t>(w, BUTTON_H));
	}

	// --- drawing -----------------------------------------------------------

	void UITrade::draw_cell(Rectangle<int16_t> box, int32_t item_id, int16_t count,
		bool lit) const
	{
		GraphicsGL::get().drawrectangle(
			box.left(), box.top(), box.width(), box.height(),
			lit ? 0.20f : 0.13f,
			lit ? 0.30f : 0.15f,
			lit ? 0.22f : 0.19f, 0.92f);

		if (!item_id)
			return;

		const Texture& icon = ItemData::get(item_id).get_icon(false);

		if (icon.is_valid())
		{
			// Icons are 32 square and the cells are 26, so they are scaled to
			// fit rather than clipped - a clipped icon loses the very corner
			// that tells two potions apart.
			//
			// FIT BY THE TALLER SIDE. Scaling by width alone let anything
			// taller than it is wide - a book, a weapon - come out longer
			// than the cell and spill into the row above and below.
			Point<int16_t> size = icon.get_dimensions();

			double fit = 1.0;

			if (size.x() > 0)
				fit = static_cast<double>(box.width() - 4) / size.x();

			if (size.y() > 0)
			{
				double tall = static_cast<double>(box.height() - 4) / size.y();

				if (tall < fit)
					fit = tall;
			}

			if (fit > 1.0)
				fit = 1.0;

			Point<int16_t> to(
				static_cast<int16_t>(size.x() * fit),
				static_cast<int16_t>(size.y() * fit));

			// AN ITEM ICON HANGS FROM ITS BOTTOM LEFT.
			//
			// Texture::draw subtracts the texture's origin from the position
			// - and does it BEFORE the stretch, so the shift is in unscaled
			// pixels. Item icons carry an origin at their bottom, so drawing
			// one at the top of a cell put it a whole icon-height ABOVE the
			// cell: on the second screen every item in the strip floated up
			// over the meso buttons and hid them.
			//
			// UICashShop works around this by adding a literal 32. The origin
			// is the real number and it is right here, so this asks for it.
			Point<int16_t> origin = icon.get_origin();

			icon.draw(DrawArgument(Point<int16_t>(
				static_cast<int16_t>(box.left() + (box.width() - to.x()) / 2 + origin.x()),
				static_cast<int16_t>(box.top() + (box.height() - to.y()) / 2 + origin.y())), to));
		}

		if (count > 1)
		{
			count_text.change_text(std::to_string(count));
			count_text.draw(Point<int16_t>(
				static_cast<int16_t>(box.right() - 2),
				static_cast<int16_t>(box.bottom() - 15)));
		}
	}

	// One side's offering, written out: "YOU: 10 Red Potion, Wooden Sword".
	//
	// Two lines at most - anything longer is trimmed rather than allowed to
	// run out of the window, because a name that overflows the frame looks
	// like a fault in a way that "..." does not.
	void UITrade::draw_contents(bool is_mine, int16_t y) const
	{
		const std::array<Held, TABLE>& side = is_mine ? mine : theirs;

		std::string listed;

		for (size_t i = 0; i < TABLE; i++)
		{
			if (side[i].id == 0 || side[i].count <= 0)
				continue;

			if (!listed.empty())
				listed += ", ";

			if (side[i].count > 1)
				listed += std::to_string(side[i].count) + " ";

			listed += ItemData::get(side[i].id).get_name();
		}

		if (listed.empty())
			listed = "nothing yet";

		small.change_text((is_mine ? "YOU: " : "THEM: ") + listed);
		small.draw(position + Point<int16_t>(6, y));
	}

	void UITrade::draw(float alpha) const
	{
		// SOMETHING TO READ THE TEXT AGAINST.
		//
		// The panel has its own plate; the main screen has the map, which is
		// bright, moving, and the worst possible backing for white text. On a
		// device with no second screen this window was simply laid over the
		// scenery - "sitting on the background".
		if (!in_panel)
		{
			constexpr int16_t PAD = 10;

			GraphicsGL::get().drawrectangle(
				static_cast<int16_t>(position.x() - PAD),
				static_cast<int16_t>(position.y() - PAD),
				static_cast<int16_t>(room.x() + PAD * 2),
				static_cast<int16_t>(room.y() + PAD * 2),
				0.07f, 0.08f, 0.11f, 0.96f);

			// A border, so it reads as a window and not as a dark patch.
			GraphicsGL::get().drawrectangle(
				static_cast<int16_t>(position.x() - PAD),
				static_cast<int16_t>(position.y() - PAD),
				static_cast<int16_t>(room.x() + PAD * 2), 2,
				0.45f, 0.40f, 0.28f, 1.0f);

			GraphicsGL::get().drawrectangle(
				static_cast<int16_t>(position.x() - PAD),
				static_cast<int16_t>(position.y() + room.y() + PAD - 2),
				static_cast<int16_t>(room.x() + PAD * 2), 2,
				0.45f, 0.40f, 0.28f, 1.0f);
		}

		if (!open)
		{
			// Not in a trade. The page still says something rather than being
			// a blank rectangle, because arriving at an empty screen is how
			// somebody concludes a feature is broken.
			label.change_text(status.empty()
				? "No trade open."
				: status);

			label.draw(position + Point<int16_t>(
				static_cast<int16_t>(room.x() / 2),
				static_cast<int16_t>(room.y() / 2 - 20)));

			small.change_text("Social > Trade lists who is here.");
			small.draw(position + Point<int16_t>(10,
				static_cast<int16_t>(room.y() / 2 + 4)));

			return;
		}

		title.change_text(joined
			? ("TRADE WITH " + partner)
			: "WAITING FOR AN ANSWER");

		title.draw(position + Point<int16_t>(
			static_cast<int16_t>(room.x() / 2), TITLE_Y));

		int16_t both = static_cast<int16_t>(GRID_W * 2 + GRID_GAP);
		int16_t left = static_cast<int16_t>((room.x() - both) / 2);

		label.change_text(i_confirmed ? "YOU - locked" : "YOU");
		label.draw(position + Point<int16_t>(
			static_cast<int16_t>(left + GRID_W / 2), HEAD_Y));

		label.change_text(they_confirmed ? "THEM - locked" : "THEM");
		label.draw(position + Point<int16_t>(
			static_cast<int16_t>(left + GRID_W + GRID_GAP + GRID_W / 2), HEAD_Y));

		for (size_t i = 0; i < TABLE; i++)
		{
			draw_cell(table_cell(true, i), mine[i].id, mine[i].count, false);
			draw_cell(table_cell(false, i), theirs[i].id, theirs[i].count, false);
		}

		small.change_text(with_commas(my_meso) + " meso");
		small.draw(position + Point<int16_t>(left, MESO_Y));

		small.change_text(with_commas(their_meso) + " meso");
		small.draw(position + Point<int16_t>(
			static_cast<int16_t>(left + GRID_W + GRID_GAP), MESO_Y));

		// WHAT IS ACTUALLY ON THE TABLE, IN WORDS.
		//
		// The grids show an icon and a count, and 26 pixels of icon is not
		// enough to tell two potions apart - "almost impossible to figure out
		// what is being traded". The names go below everything else, in the
		// space the taller main-screen box has and the panel does not, so the
		// second screen's layout is untouched.
		// WHAT IS ON THE TABLE, IN WORDS, ON EVERY SCREEN.
		//
		// These used to need forty spare pixels and so never appeared on the
		// second screen at all - which is the screen the trade is actually
		// conducted on. The cells above were shrunk to pay for them.
		//
		// Still guarded: if a layout ever leaves no room, the lines are
		// skipped rather than drawn over the buttons.
		constexpr int16_t NAMES_Y = BUTTON_Y + BUTTON_H + 3;
		constexpr int16_t NAMES_H = 12;

		if (room.y() >= NAMES_Y + NAMES_H * 2)
		{
			draw_contents(true, static_cast<int16_t>(NAMES_Y));
			draw_contents(false, static_cast<int16_t>(NAMES_Y + NAMES_H));
		}

		// THE MONEY BUTTONS, and they go dead once locked - the server
		// refuses a change after a confirm, so offering one would be a button
		// that does nothing.
		bool can_change = joined && !i_confirmed;

		for (size_t i = 0; i < 4; i++)
		{
			Rectangle<int16_t> box = meso_button(i);

			GraphicsGL::get().drawrectangle(
				box.left(), box.top(), box.width(), box.height(),
				can_change ? 0.17f : 0.10f,
				can_change ? 0.19f : 0.11f,
				can_change ? 0.23f : 0.13f, 1.0f);

			label.change_text(i < 3
				? ("+" + std::to_string(MESO_STEPS[i] / 1000) + "k")
				: "clear");

			label.draw(Point<int16_t>(
				static_cast<int16_t>(box.left() + box.width() / 2),
				static_cast<int16_t>(box.top() + 3)));
		}

		// YOUR OWN THINGS, ONE ROW, TAP TO PUT ONE DOWN.
		for (size_t i = 0; i < STRIP; i++)
		{
			size_t at = strip_from + i;

			if (at < offers.size())
				draw_cell(strip_cell(i), offers[at].id, offers[at].count, can_change);
			else
				draw_cell(strip_cell(i), 0, 0, false);
		}

		for (bool right : { false, true })
		{
			Rectangle<int16_t> box = strip_arrow(right);

			bool live = right
				? (strip_from + STRIP < offers.size())
				: (strip_from > 0);

			GraphicsGL::get().drawrectangle(
				box.left(), box.top(), box.width(), box.height(),
				live ? 0.17f : 0.10f,
				live ? 0.19f : 0.11f,
				live ? 0.23f : 0.13f, 1.0f);

			label.change_text(right ? ">" : "<");
			label.draw(Point<int16_t>(
				static_cast<int16_t>(box.left() + box.width() / 2),
				static_cast<int16_t>(box.top() + 5)));
		}

		Rectangle<int16_t> go = confirm_button();

		bool can_confirm = joined && !i_confirmed;

		GraphicsGL::get().drawrectangle(
			go.left(), go.top(), go.width(), go.height(),
			can_confirm ? 0.16f : 0.10f,
			can_confirm ? 0.30f : 0.11f,
			can_confirm ? 0.20f : 0.13f, 1.0f);

		label.change_text(i_confirmed ? "WAITING" : "CONFIRM");
		label.draw(Point<int16_t>(
			static_cast<int16_t>(go.left() + go.width() / 2),
			static_cast<int16_t>(go.top() + 5)));

		Rectangle<int16_t> stop = cancel_button();

		GraphicsGL::get().drawrectangle(
			stop.left(), stop.top(), stop.width(), stop.height(),
			0.30f, 0.13f, 0.13f, 1.0f);

		label.change_text("CANCEL");
		label.draw(Point<int16_t>(
			static_cast<int16_t>(stop.left() + stop.width() / 2),
			static_cast<int16_t>(stop.top() + 5)));

		if (!status.empty())
		{
			small.change_text(status);
			small.draw(position + Point<int16_t>(10,
				static_cast<int16_t>(BUTTON_Y + BUTTON_H + 2)));
		}
	}

	// --- what it does ------------------------------------------------------

	void UITrade::rebuild_offers()
	{
		offers.clear();

		if (!Stage::get().is_active())
			return;

		const Inventory& bag = Stage::get().get_player().get_inventory();

		// CASH IS MISSING ON PURPOSE. The server refuses cash items and pets
		// outright - "Cash items are not allowed to be traded" - so listing
		// them would offer a tap that can only ever fail.
		for (InventoryType::Id type : {
			InventoryType::Id::EQUIP,
			InventoryType::Id::USE,
			InventoryType::Id::SETUP,
			InventoryType::Id::ETC })
		{
			int16_t last = static_cast<int16_t>(bag.get_slotmax(type));

			for (int16_t slot = 1; slot <= last; slot++)
			{
				int32_t id = bag.get_item_id(type, slot);

				if (!id)
					continue;

				Offer offer;

				offer.type = type;
				offer.slot = slot;
				offer.id = id;
				offer.count = bag.get_item_count(type, slot);

				offers.push_back(offer);
			}
		}

		if (strip_from >= offers.size())
			strip_from = 0;
	}

	void UITrade::place(const Offer& offer)
	{
		if (next_free >= static_cast<int8_t>(TABLE))
		{
			status = "The table is full.";

			return;
		}

		// The whole stack. Splitting a stack needs a number typed in, and on
		// a screen with no keyboard that is a dialog for a case that almost
		// never comes up between two people at the same table.
		TradeSetItemPacket(offer.type, offer.slot,
			offer.count > 0 ? offer.count : 1,
			static_cast<int8_t>(next_free + 1)).dispatch();

		status.clear();
	}

	void UITrade::add_meso(int32_t amount)
	{
		int64_t held = Stage::get().is_active()
			? Stage::get().get_player().get_inventory().get_meso()
			: 0;

		int64_t want = (amount == 0)
			? 0
			: static_cast<int64_t>(my_meso) + amount;

		// Never offer more than is in the purse. The server would refuse it
		// and say nothing about why.
		if (want > held)
			want = held;

		if (want < 0)
			want = 0;

		if (want == my_meso)
			return;

		my_meso = static_cast<int32_t>(want);

		TradeSetMesoPacket(my_meso).dispatch();
	}

	void UITrade::confirm()
	{
		i_confirmed = true;

		status = they_confirmed
			? "Finishing..."
			: "Waiting for " + partner + ".";

		TradeConfirmPacket().dispatch();
	}

	void UITrade::cancel()
	{
		TradeExitPacket().dispatch();

		open = false;
		joined = false;
		status = "You cancelled the trade.";
	}

	void UITrade::update()
	{
		UIElement::update();

		if (!open)
		{
			// Count the result down, then get out of the way.
			//
			// "Trade complete." used to sit on the screen until something
			// else moved it - on the second screen that meant for ever,
			// because nothing else ever did.
			if (dismiss_in > 0 && --dismiss_in == 0)
			{
				finished = true;

				// On the main screen we are a window and can simply go. In
				// the panel we are a page: the panel watches is_finished()
				// and turns back, because a page that removed itself would
				// leave the second screen blank.
				if (!in_panel)
					deactivate();
			}

			return;
		}

		if (--until_rebuild > 0)
			return;

		until_rebuild = 30;

		rebuild_offers();
	}

	Cursor::State UITrade::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (!open)
			return UIElement::send_cursor(clicked, cursorpos);

		if (cancel_button().contains(cursorpos))
		{
			if (clicked)
				cancel();

			return Cursor::State::CANCLICK;
		}

		bool can_change = joined && !i_confirmed;

		if (confirm_button().contains(cursorpos))
		{
			if (clicked && can_change)
				confirm();

			return Cursor::State::CANCLICK;
		}

		if (!can_change)
			return UIElement::send_cursor(clicked, cursorpos);

		for (size_t i = 0; i < 4; i++)
		{
			if (!meso_button(i).contains(cursorpos))
				continue;

			if (clicked)
				add_meso(i < 3 ? MESO_STEPS[i] : 0);

			return Cursor::State::CANCLICK;
		}

		for (bool right : { false, true })
		{
			if (!strip_arrow(right).contains(cursorpos))
				continue;

			if (clicked)
			{
				if (right && strip_from + STRIP < offers.size())
					strip_from += STRIP;
				else if (!right && strip_from >= STRIP)
					strip_from -= STRIP;
				else if (!right)
					strip_from = 0;
			}

			return Cursor::State::CANCLICK;
		}

		for (size_t i = 0; i < STRIP; i++)
		{
			if (!strip_cell(i).contains(cursorpos))
				continue;

			size_t at = strip_from + i;

			if (clicked && at < offers.size())
				place(offers[at]);

			return Cursor::State::CANCLICK;
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}
}
