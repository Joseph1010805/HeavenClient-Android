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
#include "UIEquipInventory.h"

#include "../UI.h"

#include "../Components/MapleButton.h"
#include "../Data/ItemData.h"
#include "../../Graphics/GraphicsGL.h"
#include "../Audio/Audio.h"

#include "../Net/Packets/InventoryPackets.h"
#include "../IO/UITypes/UIItemInventory.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UIEquipInventory::UIEquipInventory(const Inventory& invent) : UIDragElement<PosEQINV>(), inventory(invent), tab(Buttons::BT_TAB1), hasPendantSlot(false), hasPocketSlot(false)
	{
		// Column 1
		iconpositions[Equipslot::Id::RING1] = Point<int16_t>(14, 50);
		iconpositions[Equipslot::Id::RING2] = Point<int16_t>(14, 91);
		iconpositions[Equipslot::Id::RING3] = Point<int16_t>(14, 132);
		iconpositions[Equipslot::Id::RING4] = Point<int16_t>(14, 173);
		iconpositions[Equipslot::Id::POCKET] = Point<int16_t>(14, 214);
		iconpositions[Equipslot::Id::BOOK] = Point<int16_t>(14, 255);

		// Column 2
		//iconpositions[Equipslot::Id::NONE] = Point<int16_t>(55, 50);
		iconpositions[Equipslot::Id::PENDANT2] = Point<int16_t>(55, 91);
		iconpositions[Equipslot::Id::PENDANT1] = Point<int16_t>(55, 132);
		iconpositions[Equipslot::Id::WEAPON] = Point<int16_t>(55, 173);
		iconpositions[Equipslot::Id::BELT] = Point<int16_t>(55, 214);
		//iconpositions[Equipslot::Id::NONE] = Point<int16_t>(55, 255);

		// Column 3
		iconpositions[Equipslot::Id::HAT] = Point<int16_t>(96, 50);
		iconpositions[Equipslot::Id::FACE] = Point<int16_t>(96, 91);
		iconpositions[Equipslot::Id::EYEACC] = Point<int16_t>(96, 132);
		iconpositions[Equipslot::Id::TOP] = Point<int16_t>(96, 173);
		iconpositions[Equipslot::Id::BOTTOM] = Point<int16_t>(96, 214);
		iconpositions[Equipslot::Id::SHOES] = Point<int16_t>(96, 255);

		// Column 4
		//iconpositions[Equipslot::Id::NONE] = Point<int16_t>(137, 50);
		//iconpositions[Equipslot::Id::NONE] = Point<int16_t>(137, 91);
		iconpositions[Equipslot::Id::EARACC] = Point<int16_t>(137, 132);
		iconpositions[Equipslot::Id::SHOULDER] = Point<int16_t>(137, 173);
		iconpositions[Equipslot::Id::GLOVES] = Point<int16_t>(137, 214);
		iconpositions[Equipslot::Id::ANDROID_SLOT] = Point<int16_t>(137, 255);

		// Column 5
		iconpositions[Equipslot::Id::EMBLEM] = Point<int16_t>(178, 50);
		iconpositions[Equipslot::Id::BADGE] = Point<int16_t>(178, 91);
		iconpositions[Equipslot::Id::MEDAL] = Point<int16_t>(178, 132);
		iconpositions[Equipslot::Id::SUBWEAPON] = Point<int16_t>(178, 173);
		iconpositions[Equipslot::Id::CAPE] = Point<int16_t>(178, 214);
		iconpositions[Equipslot::Id::HEART] = Point<int16_t>(178, 255);

		//iconpositions[Equipslot::Id::SHIELD] = Point<int16_t>(142, 124);
		//iconpositions[Equipslot::Id::TAMEDMOB] = Point<int16_t>(142, 91);
		//iconpositions[Equipslot::Id::SADDLE] = Point<int16_t>(76, 124);

		tab_source[Buttons::BT_TAB0] = "Equip";
		tab_source[Buttons::BT_TAB1] = "Cash";
		tab_source[Buttons::BT_TAB2] = "Pet";
		tab_source[Buttons::BT_TAB3] = "Android";

		nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];
		nl::node Equip = nl::nx::ui["UIWindow4.img"]["Equip"];

		background[Buttons::BT_TAB0] = Equip[tab_source[Buttons::BT_TAB0]]["backgrnd"];
		background[Buttons::BT_TAB1] = Equip[tab_source[Buttons::BT_TAB1]]["backgrnd"];
		background[Buttons::BT_TAB2] = Equip[tab_source[Buttons::BT_TAB2]]["backgrnd"];
		background[Buttons::BT_TAB3] = Equip[tab_source[Buttons::BT_TAB3]]["backgrnd"];

		for (uint16_t i = Buttons::BT_TAB0; i < Buttons::BT_TABE; i++)
			for (auto slot : Equip[tab_source[i]]["Slots"])
				if (slot.name().find("_") == std::string::npos)
					Slots[i].emplace_back(slot);

		nl::node EquipGL = nl::nx::ui["UIWindowGL.img"]["Equip"];
		nl::node backgrnd = Equip["backgrnd"];

		Point<int16_t> bg_dimensions = Texture(backgrnd).get_dimensions();

		sprites.emplace_back(EquipGL["Totem"]["backgrnd"], Point<int16_t>(-56, 0));
		sprites.emplace_back(backgrnd);
		sprites.emplace_back(Equip["backgrnd2"]);

		tabbar = Equip["tabbar"];
		disabled = Equip[tab_source[Buttons::BT_TAB0]]["disabled"];
		disabled2 = Equip[tab_source[Buttons::BT_TAB0]]["disabled2"];

		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(close, Point<int16_t>(bg_dimensions.x() - 19, 5));
		buttons[Buttons::BT_SLOT] = std::make_unique<MapleButton>(Equip[tab_source[Buttons::BT_TAB0]]["BtSlot"]);
		buttons[Buttons::BT_EFFECT] = std::make_unique<MapleButton>(EquipGL["Equip"]["btEffect"]);
		buttons[Buttons::BT_SALON] = std::make_unique<MapleButton>(EquipGL["Equip"]["btSalon"]);
		buttons[Buttons::BT_CONSUMESETTING] = std::make_unique<MapleButton>(Equip[tab_source[Buttons::BT_TAB2]]["BtConsumeSetting"]);
		buttons[Buttons::BT_EXCEPTION] = std::make_unique<MapleButton>(Equip[tab_source[Buttons::BT_TAB2]]["BtException"]);
		buttons[Buttons::BT_SHOP] = std::make_unique<MapleButton>(Equip[tab_source[Buttons::BT_TAB3]]["BtShop"]);

		buttons[Buttons::BT_CONSUMESETTING]->set_state(Button::State::DISABLED);
		buttons[Buttons::BT_EXCEPTION]->set_state(Button::State::DISABLED);
		buttons[Buttons::BT_SHOP]->set_state(Button::State::DISABLED);

		nl::node Tab = Equip["Tab"];

		for (uint16_t i = Buttons::BT_TAB0; i < Buttons::BT_TABE; i++)
			buttons[Buttons::BT_TAB0 + i] = std::make_unique<TwoSpriteButton>(Tab["disabled"][i], Tab["enabled"][i], Point<int16_t>(0, 3));

		dimension = bg_dimensions;
		dragarea = Point<int16_t>(bg_dimensions.x(), 20);

		load_icons();
		change_tab(Buttons::BT_TAB0);
	}

	// Everything the grid shows, in reading order. Short names: a cell is 52
	// wide and "SHOULDER" is not.
	const UIEquipInventory::PanelSlot UIEquipInventory::PANEL_SLOTS[] = {
		{ Equipslot::Id::HAT,      "HAT"    },
		{ Equipslot::Id::FACE,     "FACE"   },
		{ Equipslot::Id::EYEACC,   "EYE"    },
		{ Equipslot::Id::EARACC,   "EAR"    },
		{ Equipslot::Id::TOP,      "TOP"    },
		{ Equipslot::Id::BOTTOM,   "BOTTOM" },
		{ Equipslot::Id::SHOES,    "SHOES"  },
		{ Equipslot::Id::GLOVES,   "GLOVE"  },
		{ Equipslot::Id::CAPE,     "CAPE"   },
		{ Equipslot::Id::SHIELD,   "SHIELD" },
		{ Equipslot::Id::WEAPON,   "WEAPON" },
		{ Equipslot::Id::SHOULDER, "SHLDR"  },
		{ Equipslot::Id::RING1,    "RING 1" },
		{ Equipslot::Id::RING2,    "RING 2" },
		{ Equipslot::Id::RING3,    "RING 3" },
		{ Equipslot::Id::RING4,    "RING 4" },
		{ Equipslot::Id::PENDANT1, "PEND 1" },
		{ Equipslot::Id::PENDANT2, "PEND 2" },
		{ Equipslot::Id::MEDAL,    "MEDAL"  },
		{ Equipslot::Id::BELT,     "BELT"   },
		{ Equipslot::Id::POCKET,   "POCKET" },
		{ Equipslot::Id::EMBLEM,   "EMBLEM" },
		{ Equipslot::Id::BADGE,    "BADGE"  }
	};

	const size_t UIEquipInventory::PANEL_SLOT_COUNT =
		sizeof(PANEL_SLOTS) / sizeof(PANEL_SLOTS[0]);

	// The slots a cosmetic can be worn in. Rings, medals and the rest have no
	// cash counterpart, so promising a box for one would be a lie.
	const UIEquipInventory::PanelSlot UIEquipInventory::CASH_SLOTS[] = {
		{ Equipslot::Id::HAT,    "HAT"    },
		{ Equipslot::Id::FACE,   "FACE"   },
		{ Equipslot::Id::EYEACC, "EYE"    },
		{ Equipslot::Id::EARACC, "EAR"    },
		{ Equipslot::Id::TOP,    "TOP"    },
		{ Equipslot::Id::BOTTOM, "BOTTOM" },
		{ Equipslot::Id::SHOES,  "SHOES"  },
		{ Equipslot::Id::GLOVES, "GLOVE"  },
		{ Equipslot::Id::CAPE,   "CAPE"   },
		{ Equipslot::Id::SHIELD, "SHIELD" },
		{ Equipslot::Id::WEAPON, "WEAPON" }
	};

	const size_t UIEquipInventory::CASH_SLOT_COUNT =
		sizeof(CASH_SLOTS) / sizeof(CASH_SLOTS[0]);

	bool UIEquipInventory::on_cash_tab() const
	{
		return tab == Buttons::BT_TAB1;
	}

	void UIEquipInventory::build_panel_grid()
	{
		int16_t grid_w = PANEL_COLS * PANEL_CELL_W;
		int16_t left = (panel_screen.x() - grid_w) / 2;

		// Every slot the rack knew about is forgotten first: anything left
		// with its old character-shaped position would still answer to a
		// touch there, on a part of the screen now showing something else.
		for (auto iter : iconpositions)
			iter.second = Point<int16_t>(-1000, -1000);

		for (size_t i = 0; i < PANEL_SLOT_COUNT; i++)
		{
			int16_t col = static_cast<int16_t>(i % PANEL_COLS);
			int16_t row = static_cast<int16_t>(i / PANEL_COLS);

			iconpositions[PANEL_SLOTS[i].slot] = Point<int16_t>(
				left + col * PANEL_CELL_W + PANEL_ICON_X,
				PANEL_GRID_TOP + row * PANEL_CELL_H);

			panel_slot_names[PANEL_SLOTS[i].slot] = Text(
				Text::Font::A11M, Text::Alignment::CENTER,
				Color::Name::WHITE, PANEL_SLOTS[i].name);
		}

		// The CASH tab's boxes, on the same grid. They are a different TAB,
		// not a different place, so they can reuse every coordinate.
		for (auto iter : cash_iconpositions)
			iter.second = Point<int16_t>(-1000, -1000);

		for (size_t i = 0; i < CASH_SLOT_COUNT; i++)
		{
			int16_t col = static_cast<int16_t>(i % PANEL_COLS);
			int16_t row = static_cast<int16_t>(i / PANEL_COLS);

			cash_iconpositions[CASH_SLOTS[i].slot] = Point<int16_t>(
				left + col * PANEL_CELL_W + PANEL_ICON_X,
				PANEL_GRID_TOP + row * PANEL_CELL_H);

			panel_slot_names[CASH_SLOTS[i].slot] = Text(
				Text::Font::A11M, Text::Alignment::CENTER,
				Color::Name::WHITE, CASH_SLOTS[i].name);
		}
	}

	void UIEquipInventory::set_panel(Point<int16_t> screen)
	{
		panel = true;
		panel_screen = screen;

		// The tabs move to the left so the row has room for a button. They are
		// placed rather than left where the artwork puts them, which spread
		// them across the whole width.
		Buttons tabs[] = { Buttons::BT_TAB0, Buttons::BT_TAB1,
			Buttons::BT_TAB2, Buttons::BT_TAB3 };

		for (int i = 0; i < 4; i++)
			buttons[tabs[i]]->set_position(Point<int16_t>(
				PANEL_TAB_LEFT + i * PANEL_TAB_STEP, PANEL_TAB_TOP));

		// The window's own furniture goes: a close box, the cash-shop and salon
		// buttons, the slot-expansion button. What is left is the doll and its
		// slots, which is the whole reason to give it a screen.
		buttons[Buttons::BT_CLOSE]->set_active(false);
		buttons[Buttons::BT_SLOT]->set_active(false);
		buttons[Buttons::BT_EFFECT]->set_active(false);
		buttons[Buttons::BT_SALON]->set_active(false);
		buttons[Buttons::BT_CONSUMESETTING]->set_active(false);
		buttons[Buttons::BT_EXCEPTION]->set_active(false);
		buttons[Buttons::BT_SHOP]->set_active(false);

		action_text = OutlinedText(Text::Font::A12B, Text::Alignment::CENTER,
			Color::Name::WHITE, Color::Name::TUNA);

		action_text.change_text("UNEQUIP");

		// Lay the boxes out and move every slot onto them. Last, so it
		// overwrites the rack positions set up by the constructor.
		build_panel_grid();

		// As tall as the grid needs, so the page centres it correctly.
		int16_t rows = static_cast<int16_t>(
			(PANEL_SLOT_COUNT + PANEL_COLS - 1) / PANEL_COLS);

		dimension = Point<int16_t>(screen.x(),
			PANEL_GRID_TOP + rows * PANEL_CELL_H + 8);
	}

	Rectangle<int16_t> UIEquipInventory::action_bounds() const
	{
		if (!panel)
			return Rectangle<int16_t>();

		// ABSOLUTE, not relative to the window.
		//
		// This window hit-tests its slots in the panel's own coordinates -
		// slot_by_position compares against position + the slot offset - so a
		// button measured from the window's corner was compared against a
		// cursor measured from the screen's, and never matched. That is why
		// the button did nothing.
		//
		// It sits in the tab row, to the right of the four tabs, which is the
		// only band of this window with nothing already in it.
		constexpr int16_t W = 86;
		constexpr int16_t H = 18;

		Point<int16_t> at = position + Point<int16_t>(
			PANEL_TAB_LEFT + 4 * PANEL_TAB_STEP + 6, PANEL_TAB_TOP);

		return Rectangle<int16_t>(at, at + Point<int16_t>(W, H));
	}

	void UIEquipInventory::unequip_selected()
	{
		if (selected == Equipslot::Id::NONE)
			return;

		if (!shown_icon(selected))
			return;

		// Exactly what a double click does on a desktop: find a free bag slot
		// and move it there. Nowhere to put it means nothing happens, rather
		// than an item vanishing.
		int16_t freeslot = inventory.find_free_slot(InventoryType::Id::EQUIP);

		if (!freeslot)
			return;

		// The worn slot, not the box's own: a box showing a cosmetic must
		// take the cosmetic off, leaving the real item still equipped.
		UnequipItemPacket(worn_slot(selected), freeslot).dispatch();

		Sound(Sound::Name::DRAGEND).play();

		selected = Equipslot::Id::NONE;
	}

	bool UIEquipInventory::indragrange(Point<int16_t> cursorpos) const
	{
		if (panel)
			return false;

		return UIDragElement::indragrange(cursorpos);
	}

	void UIEquipInventory::draw(float alpha) const
	{
		if (panel)
		{
			// None of the window itself. Its frame is a black border and its
			// body is a solid white plate, and the two are one picture - the
			// same reason the item grid is drawn by hand rather than stamped.
			// The rack behind it is the background here.
			//
			// The TOTEM strip goes with it. It hangs 56 pixels to the LEFT of
			// this window's own origin, so on a panel that centres what it is
			// given it was always half off the edge. Its three slots are a
			// later-version feature and are inert here.
			UIElement::draw_buttons(alpha);

			// A box per slot, captioned with what goes in it - the same shape
			// as the bag rather than the character-shaped rack, whose labels
			// live in artwork that is not drawn here.
			//
			// EQUIP shows the real gear; CASH shows the cosmetics worn over
			// it. Pet and Android are later-version features with nothing
			// behind them here, so they get no boxes - promising a slot that
			// can never fill is worse than an empty page.
			const PanelSlot* shown = on_cash_tab() ? CASH_SLOTS : PANEL_SLOTS;
			size_t shown_count = on_cash_tab() ? CASH_SLOT_COUNT : PANEL_SLOT_COUNT;
			bool has_boxes = (tab == Buttons::BT_TAB0) || on_cash_tab();

			for (size_t i = 0; has_boxes && i < shown_count; i++)
			{
				Equipslot::Id id = shown[i].slot;
				Point<int16_t> at = position + (on_cash_tab()
					? cash_iconpositions[id] : iconpositions[id]);

				GraphicsGL::get().drawrectangle(
					at.x(), at.y(), 32, 32, 1.0f, 1.0f, 1.0f, 0.14f);

				panel_slot_names[id].draw(
					Point<int16_t>(at.x() + 16, at.y() + 33));
			}

			// What is picked, marked rather than faded - nothing here greys out.
			if (selected != Equipslot::Id::NONE)
			{
				Point<int16_t> at = position + (on_cash_tab()
					? cash_iconpositions[selected] : iconpositions[selected]);

				GraphicsGL::get().drawrectangle(at.x() + 1, at.y() + 1, 32, 32,
					1.0f, 0.92f, 0.45f, 0.85f);
			}
		}
		else
		{
			UIElement::draw(alpha);

			background[tab].draw(position);
			tabbar.draw(position);

			for (auto slot : Slots[tab])
				slot.draw(position);
		}

		if (tab == Buttons::BT_TAB0)
		{
			if (!hasPendantSlot)
				disabled.draw(DrawArgument(position + iconpositions[Equipslot::Id::PENDANT2],
					panel ? PANEL_FADE : 1.0f));

			if (!hasPocketSlot)
				disabled.draw(DrawArgument(position + iconpositions[Equipslot::Id::POCKET],
					panel ? PANEL_FADE : 1.0f));

			for (auto iter : (on_cash_tab() ? cash_icons : icons))
				if (iter.second)
					iter.second->draw(position
						+ (on_cash_tab() ? cash_iconpositions[iter.first]
							: iconpositions[iter.first])
						+ Point<int16_t>(4, 4));
		}
		if (panel && (tab == Buttons::BT_TAB0 || on_cash_tab()))
		{
			Rectangle<int16_t> act = action_bounds();
			bool ready = selected != Equipslot::Id::NONE && shown_icon(selected);

			GraphicsGL::get().drawrectangle(
				act.left(), act.top(),
				act.width(), act.height(),
				ready ? 0.42f : 0.16f,
				ready ? 0.18f : 0.16f,
				ready ? 0.18f : 0.16f,
				ready ? 0.92f : 0.55f);

			action_text.draw(Point<int16_t>(
				act.left() + act.width() / 2,
				act.top() + 1));
		}

		if (tab == Buttons::BT_TAB2)
		{
			disabled2.draw(position + Point<int16_t>(113, 57));
			disabled2.draw(position + Point<int16_t>(113, 106));
			disabled2.draw(position + Point<int16_t>(113, 155));
		}
	}

	Button::State UIEquipInventory::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case Buttons::BT_CLOSE:
			toggle_active();
			break;
		case Buttons::BT_TAB0:
		case Buttons::BT_TAB1:
		case Buttons::BT_TAB2:
		case Buttons::BT_TAB3:
			change_tab(id);

			return Button::State::IDENTITY;
		default:
			break;
		}

		return Button::State::NORMAL;
	}

	// The icon a box is showing, which follows the tab the same way the slot
	// it acts on does.
	const Icon* UIEquipInventory::shown_icon(Equipslot::Id slot) const
	{
		return on_cash_tab() ? cash_icons[slot].get() : icons[slot].get();
	}

	Icon* UIEquipInventory::shown_icon(Equipslot::Id slot)
	{
		return on_cash_tab() ? cash_icons[slot].get() : icons[slot].get();
	}

	// Which slot a box acts on - decided by the TAB, not by what happens to be
	// in it. The EQUIP tab is the real gear; the CASH tab is the cosmetics
	// worn over it, 100 slots higher.
	int16_t UIEquipInventory::worn_slot(Equipslot::Id slot) const
	{
		return on_cash_tab() ? Equipslot::cash_of(slot) : slot;
	}

	void UIEquipInventory::update_slot(Equipslot::Id slot)
	{
		// Both boxes for this place on the character, every time. They are
		// two different items in two different slots and either can change
		// without the other, so refreshing only "the one showing" leaves the
		// other stale the moment the tab is switched.
		Equipslot::Id base = Equipslot::base_of(slot);

		struct { EnumMap<Equipslot::Id, std::unique_ptr<Icon>>* into; int16_t from; }
		pair[2] =
		{
			{ &icons,      static_cast<int16_t>(base) },
			{ &cash_icons, static_cast<int16_t>(Equipslot::cash_of(base)) }
		};

		for (auto& p : pair)
		{
			auto& into = *p.into;

			if (int32_t item_id = inventory.get_item_id(InventoryType::Id::EQUIPPED, p.from))
			{
				const Texture& texture = ItemData::get(item_id).get_icon(false);

				into[base] = std::make_unique<Icon>(
					std::make_unique<EquipIcon>(p.from),
					texture,
					-1
					);
			}
			else if (into[base])
			{
				into[base].release();
			}
		}

		clear_tooltip();
	}

	void UIEquipInventory::load_icons()
	{
		icons.clear();

		for (auto iter : Equipslot::values)
			update_slot(iter);
	}

	Cursor::State UIEquipInventory::send_cursor(bool pressed, Point<int16_t> cursorpos)
	{
		Cursor::State dstate = UIDragElement::send_cursor(pressed, cursorpos);

		if (dragged)
		{
			clear_tooltip();

			return dstate;
		}

		if (panel)
		{
			Rectangle<int16_t> act = action_bounds();

			if (act.contains(cursorpos))
			{
				if (pressed)
					unequip_selected();

				clear_tooltip();

				return Cursor::State::CANCLICK;
			}
		}

		Equipslot::Id slot = slot_by_position(cursorpos);

		if (auto icon = shown_icon(slot))
		{
			if (pressed)
			{
				// On the panel a press does NOT pick the item up.
				//
				// A drag hands the UI an icon it then draws at the main
				// screen's cursor - which is how a grey shirt came to be
				// floating in the corner of the top screen. A touch never
				// finishes the drag either, so it would hang there.
				if (panel)
				{
					// PICKED, not dragged - see set_panel. Tapping the one
					// already held puts it down again, so there is a way to
					// change your mind without equipping something.
					selected = (selected == slot) ? Equipslot::Id::NONE : slot;

					clear_tooltip();

					return selected == slot ? Cursor::State::GRABBING : Cursor::State::CANGRAB;
				}

				icon->start_drag(cursorpos - position - iconpositions[slot]);

				UI::get().drag_icon(icon);

				clear_tooltip();

				return Cursor::State::GRABBING;
			}
			else
			{
				show_equip(slot);

				// A closed fist over the one being held, an open hand over the
				// rest - so the pointer says which item the button will act on.
				if (panel && slot == selected)
					return Cursor::State::GRABBING;

				return Cursor::State::CANGRAB;
			}
		}
		else
		{
			clear_tooltip();

			return Cursor::State::IDLE;
		}
	}

	void UIEquipInventory::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				toggle_active();
			}
			else if (keycode == KeyAction::Id::TAB)
			{
				uint16_t newtab = tab + 1;

				if (newtab >= Buttons::BT_TABE)
					newtab = Buttons::BT_TAB0;

				change_tab(newtab);
			}
		}
	}

	UIElement::Type UIEquipInventory::get_type() const
	{
		return TYPE;
	}

	void UIEquipInventory::doubleclick(Point<int16_t> cursorpos)
	{
		Equipslot::Id slot = slot_by_position(cursorpos);

		if (shown_icon(slot))
			if (int16_t freeslot = inventory.find_free_slot(InventoryType::Id::EQUIP))
				UnequipItemPacket(worn_slot(slot), freeslot).dispatch();
	}

	bool UIEquipInventory::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		if (Equipslot::Id slot = slot_by_position(cursorpos))
			icon.drop_on_equips(slot);

		return true;
	}

	void UIEquipInventory::toggle_active()
	{
		clear_tooltip();

		UIElement::toggle_active();
	}

	void UIEquipInventory::modify(int16_t pos, int8_t mode, int16_t arg)
	{
		// A cosmetic arrives here as 101, 102, 103 - numbers no `Equipslot`
		// has - and `by_id` answered NONE for every one of them, so putting a
		// mask on refreshed nothing and the box it belonged in stayed empty.
		// Fold to the base; update_slot does both halves anyway.
		Equipslot::Id eqpos = Equipslot::base_of(pos);
		Equipslot::Id eqarg = Equipslot::base_of(arg);

		switch (mode)
		{
		case 0:
		case 3:
			update_slot(eqpos);
			break;
		case 2:
			update_slot(eqpos);
			update_slot(eqarg);
			break;
		}
	}

	void UIEquipInventory::show_equip(Equipslot::Id slot)
	{
		UI::get().show_equip(Tooltip::Parent::EQUIPINVENTORY, slot);
	}

	void UIEquipInventory::clear_tooltip()
	{
		UI::get().clear_tooltip(Tooltip::Parent::EQUIPINVENTORY);
	}

	Equipslot::Id UIEquipInventory::slot_by_position(Point<int16_t> cursorpos) const
	{
		if (tab != Buttons::BT_TAB0 && !on_cash_tab())
			return Equipslot::Id::NONE;

		// Same boxes, different tab. The returned slot is always the BASE
		// one; what it acts on is decided by worn_slot().
		for (auto iter : (on_cash_tab() ? cash_iconpositions : iconpositions))
		{
			Rectangle<int16_t> iconrect = Rectangle<int16_t>(
				position + iter.second,
				position + iter.second + Point<int16_t>(32, 32)
				);

			if (iconrect.contains(cursorpos))
				return iter.first;
		}

		return Equipslot::Id::NONE;
	}

	void UIEquipInventory::change_tab(uint16_t tabid)
	{
		uint8_t oldtab = tab;
		tab = tabid;

		if (oldtab != tab)
		{
			clear_tooltip();

			buttons[oldtab]->set_state(Button::State::NORMAL);
			buttons[tab]->set_state(Button::State::PRESSED);

			if (tab == Buttons::BT_TAB0)
				buttons[Buttons::BT_SLOT]->set_active(true);
			else
				buttons[Buttons::BT_SLOT]->set_active(false);

			if (tab == Buttons::BT_TAB2)
			{
				buttons[Buttons::BT_CONSUMESETTING]->set_active(true);
				buttons[Buttons::BT_EXCEPTION]->set_active(true);
			}
			else
			{
				buttons[Buttons::BT_CONSUMESETTING]->set_active(false);
				buttons[Buttons::BT_EXCEPTION]->set_active(false);
			}

			if (tab == Buttons::BT_TAB3)
				buttons[Buttons::BT_SHOP]->set_active(true);
			else
				buttons[Buttons::BT_SHOP]->set_active(false);
		}
	}

	UIEquipInventory::EquipIcon::EquipIcon(int16_t s)
	{
		source = s;
	}

	void UIEquipInventory::EquipIcon::drop_on_stage() const
	{
		Sound(Sound::Name::DRAGEND).play();
	}

	void UIEquipInventory::EquipIcon::drop_on_equips(Equipslot::Id slot) const
	{
		if (Equipslot::base_of(source) == slot)
			Sound(Sound::Name::DRAGEND).play();
	}

	bool UIEquipInventory::EquipIcon::drop_on_items(InventoryType::Id tab, Equipslot::Id eqslot, int16_t slot, bool equip) const
	{
		if (tab != InventoryType::Id::EQUIP)
		{
			if (auto iteminventory = UI::get().get_element<UIItemInventory>())
			{
				if (iteminventory->is_active())
				{
					iteminventory->change_tab(InventoryType::Id::EQUIP);
					return false;
				}
			}
		}

		if (equip)
		{
			// Same as the other direction: match on where the item belongs,
			// since a cosmetic's slot is 100 above the box it came out of.
			if (Equipslot::base_of(eqslot) == Equipslot::base_of(source))
				EquipItemPacket(slot, eqslot).dispatch();
		}
		else
		{
			UnequipItemPacket(source, slot).dispatch();
		}

		return true;
	}

	Icon::IconType UIEquipInventory::EquipIcon::get_type()
	{
		return Icon::IconType::EQUIP;
	}
}