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
#include "UIItemInventory.h"
#include "UINotice.h"

#include "../UI.h"

#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"
#include "../Data/ItemData.h"
#include "../Audio/Audio.h"
#include "../Character/Player.h"
#include "../Gameplay/Stage.h"
#include "../Data/EquipData.h"
#include "../../Graphics/GraphicsGL.h"

#include "../IO/UITypes/UIKeyConfig.h"
#include "../Net/Packets/InventoryPackets.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UIItemInventory::UIItemInventory(const Inventory& invent) : UIDragElement<PosINV>(), inventory(invent), ignore_tooltip(false), tab(InventoryType::Id::EQUIP), sort_enabled(false)
	{
		nl::node Item = nl::nx::ui["UIWindow2.img"]["Item"];

		// TODO: Change these to production
		backgrnd = Item["backgrnd"];
		backgrnd2 = Item["productionBackgrnd2"];
		backgrnd3 = Item["backgrnd3"];

		full_backgrnd = Item["FullBackgrnd"];
		full_backgrnd2 = Item["FullBackgrnd2"];
		full_backgrnd3 = Item["FullBackgrnd3"];

		bg_dimensions = backgrnd.get_dimensions();
		bg_full_dimensions = full_backgrnd.get_dimensions();

		nl::node New = Item["New"];
		newitemslot = New["inventory"];
		newitemtab = New["Tab0"];

		projectile = Item["activeIcon"];
		disabled = Item["disabled"];

		nl::node Tab = Item["Tab"];
		nl::node taben = Tab["enabled"];
		nl::node tabdis = Tab["disabled"];

		nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];
		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(close);

		buttons[Buttons::BT_TAB_EQUIP] = std::make_unique<TwoSpriteButton>(tabdis["0"], taben["0"]);
		buttons[Buttons::BT_TAB_USE] = std::make_unique<TwoSpriteButton>(tabdis["1"], taben["1"]);
		buttons[Buttons::BT_TAB_ETC] = std::make_unique<TwoSpriteButton>(tabdis["2"], taben["2"]);
		buttons[Buttons::BT_TAB_SETUP] = std::make_unique<TwoSpriteButton>(tabdis["3"], taben["3"]);
		buttons[Buttons::BT_TAB_CASH] = std::make_unique<TwoSpriteButton>(tabdis["4"], taben["4"]);

		buttons[Buttons::BT_COIN] = std::make_unique<MapleButton>(Item["BtCoin3"]);
		buttons[Buttons::BT_POINT] = std::make_unique<MapleButton>(Item["BtPoint0"]);
		buttons[Buttons::BT_GATHER] = std::make_unique<MapleButton>(Item["BtGather3"]);
		buttons[Buttons::BT_SORT] = std::make_unique<MapleButton>(Item["BtSort3"]);
		buttons[Buttons::BT_FULL] = std::make_unique<MapleButton>(Item["BtFull3"]);
		buttons[Buttons::BT_SMALL] = std::make_unique<MapleButton>(Item["BtSmall3"]);
		buttons[Buttons::BT_POT] = std::make_unique<MapleButton>(Item["BtPot3"]);
		buttons[Buttons::BT_UPGRADE] = std::make_unique<MapleButton>(Item["BtUpgrade3"]);
		buttons[Buttons::BT_APPRAISE] = std::make_unique<MapleButton>(Item["BtAppraise3"]);
		buttons[Buttons::BT_EXTRACT] = std::make_unique<MapleButton>(Item["BtExtract3"]);
		buttons[Buttons::BT_DISASSEMBLE] = std::make_unique<MapleButton>(Item["BtDisassemble3"]);
		buttons[Buttons::BT_TOAD] = std::make_unique<MapleButton>(Item["BtToad3"]);

		buttons[Buttons::BT_COIN_SM] = std::make_unique<MapleButton>(Item["BtCoin4"]);
		buttons[Buttons::BT_POINT_SM] = std::make_unique<MapleButton>(Item["BtPoint1"]);
		buttons[Buttons::BT_GATHER_SM] = std::make_unique<MapleButton>(Item["BtGather4"]);
		buttons[Buttons::BT_SORT_SM] = std::make_unique<MapleButton>(Item["BtSort4"]);
		buttons[Buttons::BT_FULL_SM] = std::make_unique<MapleButton>(Item["BtFull4"]);
		buttons[Buttons::BT_SMALL_SM] = std::make_unique<MapleButton>(Item["BtSmall4"]);
		buttons[Buttons::BT_POT_SM] = std::make_unique<MapleButton>(Item["BtPot4"]);
		buttons[Buttons::BT_UPGRADE_SM] = std::make_unique<MapleButton>(Item["BtUpgrade4"]);
		buttons[Buttons::BT_APPRAISE_SM] = std::make_unique<MapleButton>(Item["BtAppraise4"]);
		buttons[Buttons::BT_EXTRACT_SM] = std::make_unique<MapleButton>(Item["BtExtract4"]);
		buttons[Buttons::BT_DISASSEMBLE_SM] = std::make_unique<MapleButton>(Item["BtDisassemble4"]);
		buttons[Buttons::BT_TOAD_SM] = std::make_unique<MapleButton>(Item["BtToad4"]);
		buttons[Buttons::BT_CASHSHOP] = std::make_unique<MapleButton>(Item["BtCashshop"]);

		buttons[Buttons::BT_POT]->set_state(Button::State::DISABLED);
		buttons[Buttons::BT_POT_SM]->set_state(Button::State::DISABLED);
		buttons[Buttons::BT_EXTRACT]->set_state(Button::State::DISABLED);
		buttons[Buttons::BT_EXTRACT_SM]->set_state(Button::State::DISABLED);
		buttons[Buttons::BT_DISASSEMBLE]->set_state(Button::State::DISABLED);
		buttons[Buttons::BT_DISASSEMBLE_SM]->set_state(Button::State::DISABLED);
		buttons[button_by_tab(tab)]->set_state(Button::State::PRESSED);

		mesolabel = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::BLACK);
		maplepointslabel = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::BLACK);
		maplepointslabel.change_text("0"); // TODO: Implement

		slotrange[InventoryType::Id::EQUIPPED] = { 1, 24 };
		slotrange[InventoryType::Id::EQUIP] = { 1, 24 };
		slotrange[InventoryType::Id::USE] = { 1, 24 };
		slotrange[InventoryType::Id::SETUP] = { 1, 24 };
		slotrange[InventoryType::Id::ETC] = { 1, 24 };
		slotrange[InventoryType::Id::CASH] = { 1, 24 };

		slider = Slider(
			Slider::Type::DEFAULT, Range<int16_t>(50, 245), 152, 6, 1 + inventory.get_slotmax(tab) / COLUMNS,
			[&](bool upwards)
			{
				int16_t shift = upwards ? -COLUMNS : COLUMNS;
				bool above = slotrange[tab].first + shift > 0;
				bool below = slotrange[tab].second + shift < inventory.get_slotmax(tab) + 1 + COLUMNS;

				if (above && below)
				{
					slotrange[tab].first += shift;
					slotrange[tab].second += shift;
				}
			}
		);

		set_full(false);
		clear_new();
		load_icons();
	}

	void UIItemInventory::draw(float alpha) const
	{
		UIElement::draw_sprites(alpha);

		Point<int16_t> mesolabel_pos = position + Point<int16_t>(127, 262);
		Point<int16_t> maplepointslabel_pos = position + Point<int16_t>(159, 279);

		if (panel)
		{
			// The window's own artwork is not drawn at all here.
			//
			// It is one picture carrying a black frame and a white body, so it
			// cannot be made to lose the frame and keep the body at half
			// strength - the two are the same pixels. The grid is drawn
			// instead: plain translucent cells, no frame, and the icons and
			// numbers on top of them at full strength.
			Point<int16_t> origin = position + grid_origin();

			int16_t held = inventory.get_slotmax(tab);

			for (int16_t row = 0; row < visible_rows(); row++)
			{
				for (int16_t col = 0; col < columns(); col++)
				{
					// Only slots the character actually has. The window
					// normally stamps a crossed-out square on the rest, which
					// on a grid sized to the screen rather than to the bag
					// meant a wall of crosses.
					if (row * columns() + col >= held)
						continue;

					GraphicsGL::get().drawrectangle(
						origin.x() + col * ICON_WIDTH + 1,
						origin.y() + row * ICON_HEIGHT + 1,
						ICON_WIDTH - 2, ICON_HEIGHT - 2,
						1.0f, 1.0f, 1.0f, 0.5f);
				}
			}

			// What is picked, marked by a brighter cell rather than by fading
			// everything else - nothing here is ever greyed out.
			if (selected && is_visible(selected))
			{
				Point<int16_t> at = position + get_slotpos(selected);

				GraphicsGL::get().drawrectangle(
					at.x() + 1, at.y() + 1, ICON_WIDTH - 2, ICON_HEIGHT - 2,
					1.0f, 0.92f, 0.45f, 0.85f);
			}
		}
		else if (full_enabled)
		{
			full_backgrnd.draw(position);
			full_backgrnd2.draw(position);
			full_backgrnd3.draw(position);

			mesolabel.draw(mesolabel_pos + Point<int16_t>(3, 70));
			maplepointslabel.draw(maplepointslabel_pos + Point<int16_t>(181, 53));
		}
		else
		{
			backgrnd.draw(position);
			backgrnd2.draw(position);
			backgrnd3.draw(position);

			slider.draw(position + Point<int16_t>(0, 1));

			mesolabel.draw(mesolabel_pos);
			maplepointslabel.draw(maplepointslabel_pos);
		}

		auto range = slotrange.at(tab);

		size_t numslots = inventory.get_slotmax(tab);
		size_t firstslot = full_enabled ? 1 : range.first;
		size_t lastslot = full_enabled ? MAXFULLSLOTS : range.second;

		for (size_t i = 0; i <= MAXFULLSLOTS; i++)
		{
			Point<int16_t> slotpos = full_enabled ? get_full_slotpos(i) : get_slotpos(i);

			if (icons.find(i) != icons.end())
			{
				auto& icon = icons.at(i);

				if (icon && i >= firstslot && i <= lastslot)
					icon->draw(position + slotpos);
			}
			else
			{
				if (!panel && i > numslots && i <= lastslot)
					disabled.draw(position + slotpos);
			}
		}

		int16_t bulletslot = inventory.get_bulletslot();

		if (tab == InventoryType::Id::USE && is_visible(bulletslot))
			projectile.draw(position + get_slotpos(bulletslot));

		if (tab == newtab)
		{
			newitemtab.draw(position + get_tabpos(newtab), alpha);

			if (is_visible(newslot))
				newitemslot.draw(position + get_slotpos(newslot) + Point<int16_t>(1, 1), alpha);
		}

		UIElement::draw_buttons(alpha);

		if (panel)
		{
			// EQUIP or USE, whichever this tab means. Drawn rather than built
			// from artwork: there is no button in the game's own files that
			// says either of these things.
			Rectangle<int16_t> act = action_bounds();

			if (act.width() > 0)
			{
				bool ready = selected != 0;

				GraphicsGL::get().drawrectangle(
					position.x() + act.left(), position.y() + act.top(),
					act.width(), act.height(),
					ready ? 0.16f : 0.10f,
					ready ? 0.42f : 0.16f,
					ready ? 0.18f : 0.10f,
					ready ? 0.92f : 0.55f);

				action_text.draw(Point<int16_t>(
					position.x() + act.left() + act.width() / 2,
					position.y() + act.top() + 4));
			}
		}
	}

	void UIItemInventory::update()
	{
		UIElement::update();

		newitemtab.update(6);
		newitemslot.update(6);

		std::string meso_str = std::to_string(inventory.get_meso());
		string_format::split_number(meso_str);

		mesolabel.change_text(meso_str);
	}

	void UIItemInventory::update_slot(int16_t slot)
	{
		if (int32_t item_id = inventory.get_item_id(tab, slot))
		{
			int16_t count;

			if (tab == InventoryType::Id::EQUIP)
				count = -1;
			else
				count = inventory.get_item_count(tab, slot);

			const bool untradable = ItemData::get(item_id).is_untradable();
			const bool cashitem = ItemData::get(item_id).is_cashitem();
			const Texture& texture = ItemData::get(item_id).get_icon(false);
			Equipslot::Id eqslot = inventory.find_equipslot(item_id);

			icons[slot] = std::make_unique<Icon>(
				std::make_unique<ItemIcon>(*this, tab, eqslot, slot, item_id, count, untradable, cashitem),
				texture, count
				);
		}
		else if (icons.count(slot))
		{
			icons.erase(slot);
		}
	}

	void UIItemInventory::load_icons()
	{
		icons.clear();

		uint8_t numslots = inventory.get_slotmax(tab);

		for (size_t i = 0; i <= MAXFULLSLOTS; i++)
			if (i <= numslots)
				update_slot(i);
	}

	Button::State UIItemInventory::button_pressed(uint16_t buttonid)
	{
		InventoryType::Id oldtab = tab;

		switch (buttonid)
		{
		case Buttons::BT_CLOSE:
			toggle_active();
			return Button::State::NORMAL;
		case Buttons::BT_TAB_EQUIP:
			tab = InventoryType::Id::EQUIP;
			break;
		case Buttons::BT_TAB_USE:
			tab = InventoryType::Id::USE;
			break;
		case Buttons::BT_TAB_SETUP:
			tab = InventoryType::Id::SETUP;
			break;
		case Buttons::BT_TAB_ETC:
			tab = InventoryType::Id::ETC;
			break;
		case Buttons::BT_TAB_CASH:
			tab = InventoryType::Id::CASH;
			break;
		case Buttons::BT_GATHER:
		case Buttons::BT_GATHER_SM:
			GatherItemsPacket(tab).dispatch();
			break;
		case Buttons::BT_SORT:
		case Buttons::BT_SORT_SM:
			SortItemsPacket(tab).dispatch();
			break;
		case Buttons::BT_FULL:
		case Buttons::BT_FULL_SM:
			set_full(true);
			return Button::State::NORMAL;
		case Buttons::BT_SMALL:
		case Buttons::BT_SMALL_SM:
			set_full(false);
			return Button::State::NORMAL;
		case Buttons::BT_COIN:
		case Buttons::BT_COIN_SM:
		case Buttons::BT_POINT:
		case Buttons::BT_POINT_SM:
		case Buttons::BT_POT:
		case Buttons::BT_POT_SM:
		case Buttons::BT_UPGRADE:
		case Buttons::BT_UPGRADE_SM:
		case Buttons::BT_APPRAISE:
		case Buttons::BT_APPRAISE_SM:
		case Buttons::BT_EXTRACT:
		case Buttons::BT_EXTRACT_SM:
		case Buttons::BT_DISASSEMBLE:
		case Buttons::BT_DISASSEMBLE_SM:
		case Buttons::BT_TOAD:
		case Buttons::BT_TOAD_SM:
		case Buttons::BT_CASHSHOP:
			OutPacket(OutPacket::Opcode::ENTER_CASHSHOP).dispatch();

			return Button::State::NORMAL;
		}

		if (tab != oldtab)
		{
			// Nothing carries across a tab: the slot numbers mean something
			// different here, and the button says a different word.
			selected = 0;

			if (panel)
				if (const char* label = action_label())
					action_text.change_text(label);

			uint16_t row = slotrange.at(tab).first / columns();
			slider.setrows(row, 6, 1 + inventory.get_slotmax(tab) / columns());

			buttons[button_by_tab(oldtab)]->set_state(Button::State::NORMAL);
			buttons[button_by_tab(tab)]->set_state(Button::State::PRESSED);

			load_icons();
			set_sort(false);
		}

		return Button::State::IDENTITY;
	}

	void UIItemInventory::doubleclick(Point<int16_t> cursorpos)
	{
		int16_t slot = slot_by_position(cursorpos - position);

		if (icons.count(slot) && is_visible(slot))
		{
			if (int32_t item_id = inventory.get_item_id(tab, slot))
			{
				switch (tab)
				{
				case InventoryType::Id::EQUIP:
					if (can_wear_equip(slot))
						EquipItemPacket(slot, inventory.find_equipslot(item_id)).dispatch();

					break;
				case InventoryType::Id::USE:
					UseItemPacket(slot, item_id).dispatch();
					break;
				}
			}
		}
	}

	bool UIItemInventory::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		int16_t slot = slot_by_position(cursorpos - position);

		if (slot > 0)
		{
			int32_t item_id = inventory.get_item_id(tab, slot);
			Equipslot::Id eqslot;
			bool equip;

			if (item_id && tab == InventoryType::Id::EQUIP)
			{
				eqslot = inventory.find_equipslot(item_id);
				equip = true;
			}
			else
			{
				eqslot = Equipslot::Id::NONE;
				equip = false;
			}

			ignore_tooltip = true;

			return icon.drop_on_items(tab, eqslot, slot, equip);
		}

		return true;
	}

	Cursor::State UIItemInventory::send_cursor(bool pressed, Point<int16_t> cursorpos)
	{
		Cursor::State dstate = UIDragElement::send_cursor(pressed, cursorpos);

		if (dragged)
		{
			clear_tooltip();

			return dstate;
		}

		Point<int16_t> cursor_relative = cursorpos - position;

		// The panel shows every slot at once, so there is nothing to scroll -
		// but the slider was still live and still answering to the cursor. It
		// sits right where the action button now is, and it was swallowing the
		// press before the button ever saw it.
		if (!panel && !full_enabled && slider.isenabled())
		{
			Cursor::State sstate = slider.send_cursor(cursor_relative, pressed);

			if (sstate != Cursor::State::IDLE)
			{
				clear_tooltip();

				return sstate;
			}
		}

		if (panel)
		{
			// The action button first, so a press on it is not read as a press
			// on whatever the grid has underneath.
			Rectangle<int16_t> act = action_bounds();

			if (act.width() > 0 && act.contains(cursor_relative))
			{
				if (pressed && selected)
					activate_slot(selected);

				clear_tooltip();

				return Cursor::State::CANCLICK;
			}
		}

		int16_t slot = slot_by_position(cursor_relative);
		Icon* icon = get_icon(slot);
		bool is_icon = icon && is_visible(slot);

		if (is_icon)
		{
			if (pressed)
			{
				if (panel)
				{
					// PICKED, not dragged.
					//
					// start_drag fades the slot it came from and hands the UI a
					// pointer it holds until the item is dropped somewhere.
					// A touch never drops it, so every item tapped stayed
					// faded and the pointer stayed open-handed. Here the tap
					// simply marks the slot and the pointer closes on it - and
					// tapping the marked one again puts it down.
					selected = (selected == slot) ? 0 : slot;

					clear_tooltip();

					return selected == slot ? Cursor::State::GRABBING : Cursor::State::CANGRAB;
				}

				Point<int16_t> slotpos = get_slotpos(slot);
				icon->start_drag(cursor_relative - slotpos);
				UI::get().drag_icon(icon);

				clear_tooltip();

				return Cursor::State::GRABBING;
			}
			else if (!ignore_tooltip)
			{
				show_item(slot);

				// Closed over the one that is picked, open over the rest, so
				// the pointer says which item the action button will act on.
				return (panel && slot == selected)
					? Cursor::State::GRABBING
					: Cursor::State::CANGRAB;
			}
			else
			{
				ignore_tooltip = false;

				return Cursor::State::CANGRAB;
			}
		}
		else
		{
			clear_tooltip();

			return UIElement::send_cursor(pressed, cursorpos);
		}
	}

	void UIItemInventory::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				toggle_active();
			}
			else if (keycode == KeyAction::Id::TAB)
			{
				clear_tooltip();

				InventoryType::Id newtab;

				switch (tab)
				{
				case InventoryType::Id::EQUIP:
					newtab = InventoryType::Id::USE;
					break;
				case InventoryType::Id::USE:
					newtab = InventoryType::Id::ETC;
					break;
				case InventoryType::Id::ETC:
					newtab = InventoryType::Id::SETUP;
					break;
				case InventoryType::Id::SETUP:
					newtab = InventoryType::Id::CASH;
					break;
				case InventoryType::Id::CASH:
					newtab = InventoryType::Id::EQUIP;
					break;
				}

				button_pressed(button_by_tab(newtab));
			}
		}
	}

	UIElement::Type UIItemInventory::get_type() const
	{
		return TYPE;
	}

	void UIItemInventory::modify(InventoryType::Id type, int16_t slot, int8_t mode, int16_t arg)
	{
		if (slot <= 0)
			return;

		if (type == tab)
		{
			switch (mode)
			{
			case Inventory::Modification::ADD:
				update_slot(slot);
				newtab = type;
				newslot = slot;
				break;
			case Inventory::Modification::CHANGECOUNT:
			case Inventory::Modification::ADDCOUNT:
				if (auto icon = get_icon(slot))
					icon->set_count(arg);

				break;
			case Inventory::Modification::SWAP:
				if (arg != slot)
				{
					update_slot(slot);
					update_slot(arg);
				}

				break;
			case Inventory::Modification::REMOVE:
				update_slot(slot);
				break;
			}
		}

		switch (mode)
		{
		case Inventory::Modification::ADD:
		case Inventory::Modification::ADDCOUNT:
			newtab = type;
			newslot = slot;
			break;
		case Inventory::Modification::CHANGECOUNT:
		case Inventory::Modification::SWAP:
		case Inventory::Modification::REMOVE:
			if (newslot == slot && newtab == type)
				clear_new();

			break;
		}
	}

	void UIItemInventory::set_sort(bool enabled)
	{
		sort_enabled = enabled;

		if (full_enabled)
		{
			if (sort_enabled)
			{
				buttons[Buttons::BT_SORT]->set_active(false);
				buttons[Buttons::BT_SORT_SM]->set_active(true);
				buttons[Buttons::BT_GATHER]->set_active(false);
				buttons[Buttons::BT_GATHER_SM]->set_active(false);
			}
			else
			{
				buttons[Buttons::BT_SORT]->set_active(false);
				buttons[Buttons::BT_SORT_SM]->set_active(false);
				buttons[Buttons::BT_GATHER]->set_active(false);
				buttons[Buttons::BT_GATHER_SM]->set_active(true);
			}
		}
		else
		{
			if (sort_enabled)
			{
				buttons[Buttons::BT_SORT]->set_active(true);
				buttons[Buttons::BT_SORT_SM]->set_active(false);
				buttons[Buttons::BT_GATHER]->set_active(false);
				buttons[Buttons::BT_GATHER_SM]->set_active(false);
			}
			else
			{
				buttons[Buttons::BT_SORT]->set_active(false);
				buttons[Buttons::BT_SORT_SM]->set_active(false);
				buttons[Buttons::BT_GATHER]->set_active(true);
				buttons[Buttons::BT_GATHER_SM]->set_active(false);
			}
		}
	}

	void UIItemInventory::change_tab(InventoryType::Id type)
	{
		button_pressed(button_by_tab(type));
	}

	void UIItemInventory::clear_new()
	{
		newtab = InventoryType::Id::NONE;
		newslot = 0;
	}

	void UIItemInventory::toggle_active()
	{
		UIElement::toggle_active();

		if (!active)
		{
			clear_new();
			clear_tooltip();
		}
	}

	void UIItemInventory::remove_cursor()
	{
		UIDragElement::remove_cursor();

		slider.remove_cursor();
	}

	void UIItemInventory::show_item(int16_t slot)
	{
		if (tab == InventoryType::Id::EQUIP)
		{
			UI::get().show_equip(Tooltip::Parent::ITEMINVENTORY, slot);
		}
		else
		{
			int32_t item_id = inventory.get_item_id(tab, slot);
			UI::get().show_item(Tooltip::Parent::ITEMINVENTORY, item_id);
		}
	}

	void UIItemInventory::clear_tooltip()
	{
		UI::get().clear_tooltip(Tooltip::Parent::ITEMINVENTORY);
	}

	bool UIItemInventory::is_visible(int16_t slot) const
	{
		return !is_not_visible(slot);
	}

	bool UIItemInventory::is_not_visible(int16_t slot) const
	{
		auto range = slotrange.at(tab);

		if (full_enabled)
			return slot < 1 || slot > 24;
		else
			return slot < range.first || slot > range.second;
	}

	bool UIItemInventory::can_wear_equip(int16_t slot) const
	{
		const Player& player = Stage::get().get_player();
		const CharStats& stats = player.get_stats();
		const CharLook& look = player.get_look();
		const bool alerted = look.get_alerted();

		if (alerted)
		{
			UI::get().emplace<UIOk>("You cannot complete this action right now.\\nEvade the attack and try again.", [](bool) {});
			return false;
		}

		const int32_t item_id = inventory.get_item_id(InventoryType::Id::EQUIP, slot);
		const EquipData& equipdata = EquipData::get(item_id);
		const ItemData& itemdata = equipdata.get_itemdata();

		const int8_t reqGender = itemdata.get_gender();
		const bool female = stats.get_female();

		switch (reqGender)
		{
		case 0: // Male
			if (female)
				return false;

			break;
		case 1: // Female
			if (!female)
				return false;

			break;
		case 2: // Unisex
		default:
			break;
		}

		const std::string jobname = stats.get_jobname();

		if (jobname == "GM" || jobname == "SuperGM")
			return true;

		int16_t reqJOB = equipdata.get_reqstat(Maplestat::Id::JOB);

		if (!stats.get_job().is_sub_job(reqJOB))
		{
			UI::get().emplace<UIOk>("Your current job\\ncannot equip the selected item.", [](bool) {});
			return false;
		}

		int16_t reqLevel = equipdata.get_reqstat(Maplestat::Id::LEVEL);
		int16_t reqDEX = equipdata.get_reqstat(Maplestat::Id::DEX);
		int16_t reqSTR = equipdata.get_reqstat(Maplestat::Id::STR);
		int16_t reqLUK = equipdata.get_reqstat(Maplestat::Id::LUK);
		int16_t reqINT = equipdata.get_reqstat(Maplestat::Id::INT);
		int16_t reqFAME = equipdata.get_reqstat(Maplestat::Id::FAME);

		int8_t i = 0;

		if (reqLevel > stats.get_stat(Maplestat::Id::LEVEL))
			i++;
		else if (reqDEX > stats.get_total(Equipstat::Id::DEX))
			i++;
		else if (reqSTR > stats.get_total(Equipstat::Id::STR))
			i++;
		else if (reqLUK > stats.get_total(Equipstat::Id::LUK))
			i++;
		else if (reqINT > stats.get_total(Equipstat::Id::INT))
			i++;
		else if (reqFAME > stats.get_honor())
			i++;

		if (i > 0)
		{
			UI::get().emplace<UIOk>("Your stats are too low to equip this item\\nor you do not meet the job requirement.", [](bool) {});
			return false;
		}

		return true;
	}

	int16_t UIItemInventory::slot_by_position(Point<int16_t> cursorpos) const
	{
		if (panel)
		{
			Point<int16_t> origin = grid_origin();

			int16_t px = cursorpos.x() - origin.x();
			int16_t py = cursorpos.y() - origin.y();

			if (px < 0 || py < 0
				|| px >= columns() * ICON_WIDTH
				|| py >= visible_rows() * ICON_HEIGHT)
				return 0;

			int16_t at = slotrange.at(tab).first
				+ (px / ICON_WIDTH) + columns() * (py / ICON_HEIGHT);

			return is_visible(at) ? at : 0;
		}

		int16_t xoff = cursorpos.x() - 11;
		int16_t yoff = cursorpos.y() - 51;

		if (xoff < 1 || xoff > 143 || yoff < 1)
			return 0;

		int16_t slot = (full_enabled ? 1 : slotrange.at(tab).first) + (xoff / ICON_WIDTH) + COLUMNS * (yoff / ICON_HEIGHT);

		return is_visible(slot) ? slot : 0;
	}

	Point<int16_t> UIItemInventory::get_slotpos(int16_t slot) const
	{
		int16_t absslot = slot - slotrange.at(tab).first;

		if (panel)
			return grid_origin() + Point<int16_t>(
				(absslot % columns()) * ICON_WIDTH,
				(absslot / columns()) * ICON_HEIGHT);

		return Point<int16_t>(
			10 + (absslot % COLUMNS) * ICON_WIDTH,
			51 + (absslot / COLUMNS) * ICON_HEIGHT
			);
	}

	Point<int16_t> UIItemInventory::get_full_slotpos(int16_t slot) const
	{
		int16_t absslot = slot - 1;
		div_t div = std::div(absslot, MAXSLOTS);
		int16_t new_slot = absslot - (div.quot * MAXSLOTS);
		int16_t adj_x = div.quot * COLUMNS * ICON_WIDTH;

		return Point<int16_t>(
			10 + adj_x + (new_slot % COLUMNS) * ICON_WIDTH,
			51 + (new_slot / COLUMNS) * ICON_HEIGHT
			);
	}

	Point<int16_t> UIItemInventory::get_tabpos(InventoryType::Id tb) const
	{
		int8_t fixed_tab = tb;

		switch (tb)
		{
		case InventoryType::Id::ETC:
			fixed_tab = 3;
			break;
		case InventoryType::Id::SETUP:
			fixed_tab = 4;
			break;
		}

		return Point<int16_t>(10 + ((fixed_tab - 1) * 31), 29);
	}

	uint16_t UIItemInventory::button_by_tab(InventoryType::Id tb) const
	{
		switch (tb)
		{
		case InventoryType::Id::EQUIP:
			return Buttons::BT_TAB_EQUIP;
		case InventoryType::Id::USE:
			return Buttons::BT_TAB_USE;
		case InventoryType::Id::SETUP:
			return Buttons::BT_TAB_SETUP;
		case InventoryType::Id::ETC:
			return Buttons::BT_TAB_ETC;
		default:
			return Buttons::BT_TAB_CASH;
		}
	}

	Icon* UIItemInventory::get_icon(int16_t slot)
	{
		auto iter = icons.find(slot);

		if (iter != icons.end())
			return iter->second.get();
		else
			return nullptr;
	}

	void UIItemInventory::set_panel(Point<int16_t> screen)
	{
		panel = true;

		panel_screen = screen;

		// Neither of the stock layouts - a wide one of our own. The narrow
		// window is four columns and eight rows, which on a screen wider than
		// it is tall leaves most of the space empty; the stock wide one draws
		// a grid that runs past its own background. This lays the slots out
		// across instead, and draws the grid rather than using the artwork.
		set_full(false);

		layout_panel();
	}

	void UIItemInventory::layout_panel()
	{
		if (!panel)
			return;

		// As many whole columns as the panel is wide enough for.
		panel_columns = (panel_screen.x() - PANEL_SIDE * 2) / ICON_WIDTH;

		if (panel_columns < 1)
			panel_columns = 1;

		// As wide as the panel, and only as tall as the grid, the tabs above it
		// and the action button below it need.
		dimension = Point<int16_t>(
			panel_screen.x(),
			PANEL_GRID_TOP + PANEL_ROWS * ICON_HEIGHT + PANEL_ACTION_H + 22);

		// Everything the window normally carries is furniture for a window:
		// a close box, resize handles, sort and gather and the rest. Off.
		for (auto& entry : buttons)
			if (entry.second)
				entry.second->set_active(false);

		// The tabs, spread across the top of the grid.
		Buttons tabs[] = {
			Buttons::BT_TAB_EQUIP, Buttons::BT_TAB_USE, Buttons::BT_TAB_ETC,
			Buttons::BT_TAB_SETUP, Buttons::BT_TAB_CASH
		};

		// Clear of the panel's page arrows, which sit in the top corners.
		int16_t x = PANEL_TAB_LEFT;

		for (Buttons id : tabs)
		{
			buttons[id]->set_active(true);
			buttons[id]->set_position(Point<int16_t>(x, PANEL_TAB_TOP));

			x += 31;
		}

		buttons[button_by_tab(tab)]->set_state(Button::State::PRESSED);

		// Every slot the grid can hold is on screen at once, so there is
		// nothing to scroll and no slider to draw.
		int16_t shown = columns() * visible_rows();

		for (auto& entry : slotrange)
			entry.second = { 1, shown };

		action_text = OutlinedText(Text::Font::A12B, Text::Alignment::CENTER,
			Color::Name::WHITE, Color::Name::TUNA);

		if (const char* label = action_label())
			action_text.change_text(label);
	}

	int16_t UIItemInventory::columns() const
	{
		return panel ? panel_columns : COLUMNS;
	}

	int16_t UIItemInventory::visible_rows() const
	{
		return panel ? PANEL_ROWS : ROWS;
	}

	Point<int16_t> UIItemInventory::grid_origin() const
	{
		// Centred across the window, which is itself the full width of the
		// panel - so the grid uses the space the narrow window left empty.
		return Point<int16_t>(
			(panel_screen.x() - columns() * ICON_WIDTH) / 2,
			PANEL_GRID_TOP);
	}

	Rectangle<int16_t> UIItemInventory::action_bounds() const
	{
		if (!panel || !action_label())
			return Rectangle<int16_t>();

		int16_t top = PANEL_GRID_TOP + PANEL_ROWS * ICON_HEIGHT + 10;
		int16_t left = (panel_screen.x() - PANEL_ACTION_W) / 2;

		return Rectangle<int16_t>(
			Point<int16_t>(left, top),
			Point<int16_t>(left + PANEL_ACTION_W, top + PANEL_ACTION_H));
	}

	const char* UIItemInventory::action_label() const
	{
		switch (tab)
		{
		case InventoryType::Id::EQUIP:
			return "EQUIP";
		case InventoryType::Id::USE:
			return "USE";
		default:
			// Nothing sensible to do with an etc item or a chair from here.
			return nullptr;
		}
	}

	void UIItemInventory::activate_slot(int16_t slot)
	{
		if (!icons.count(slot) || !is_visible(slot))
			return;

		int32_t item_id = inventory.get_item_id(tab, slot);

		if (!item_id)
			return;

		switch (tab)
		{
		case InventoryType::Id::EQUIP:
			if (can_wear_equip(slot))
			{
				EquipItemPacket(slot, inventory.find_equipslot(item_id)).dispatch();

				// The same sound the real client makes when an item lands in an
				// equip slot - there is no separate "equipped" one.
				Sound(Sound::Name::DRAGEND).play();

				// Let it go. Equipping SWAPS, so whatever came off arrives in
				// this very slot a moment later - and leaving the selection
				// behind meant the item you just took off appeared picked up,
				// ready to be put straight back on.
				selected = 0;
			}

			break;
		case InventoryType::Id::USE:
			UseItemPacket(slot, item_id).dispatch();

			selected = 0;
			break;
		}
	}

	Keyboard::Mapping UIItemInventory::selected_mapping() const
	{
		if (!panel || !selected)
			return {};

		int32_t item_id = inventory.get_item_id(tab, selected);

		if (!item_id)
			return {};

		// Only consumables go on a key. An equip has nothing to "use", and
		// binding one would produce a hotkey that does nothing.
		if (tab != InventoryType::Id::USE)
			return {};

		return Keyboard::Mapping(KeyType::Id::ITEM, item_id);
	}

	bool UIItemInventory::indragrange(Point<int16_t> cursorpos) const
	{
		if (panel)
			return false;

		return UIDragElement::indragrange(cursorpos);
	}

	void UIItemInventory::set_full(bool enabled)
	{
		full_enabled = enabled;

		if (full_enabled)
		{
			dimension = bg_full_dimensions;

			buttons[Buttons::BT_FULL]->set_active(false);
			buttons[Buttons::BT_FULL_SM]->set_active(false);
			buttons[Buttons::BT_SMALL]->set_active(false);
			buttons[Buttons::BT_SMALL_SM]->set_active(true);
		}
		else
		{
			dimension = bg_dimensions;

			buttons[Buttons::BT_FULL]->set_active(true);
			buttons[Buttons::BT_FULL_SM]->set_active(false);
			buttons[Buttons::BT_SMALL]->set_active(false);
			buttons[Buttons::BT_SMALL_SM]->set_active(false);
		}

		dragarea = Point<int16_t>(dimension.x(), 20);

		int16_t adj_x = full_enabled ? 20 : 22;
		buttons[Buttons::BT_CLOSE]->set_position(Point<int16_t>(dimension.x() - adj_x, 6));

		buttons[Buttons::BT_COIN]->set_active(!enabled);
		buttons[Buttons::BT_POINT]->set_active(!enabled);
		buttons[Buttons::BT_POT]->set_active(!enabled);
		buttons[Buttons::BT_UPGRADE]->set_active(!enabled);
		buttons[Buttons::BT_APPRAISE]->set_active(!enabled);
		buttons[Buttons::BT_EXTRACT]->set_active(!enabled);
		buttons[Buttons::BT_DISASSEMBLE]->set_active(!enabled);
		buttons[Buttons::BT_TOAD]->set_active(!enabled);
		buttons[Buttons::BT_CASHSHOP]->set_active(!enabled);

		buttons[Buttons::BT_COIN_SM]->set_active(enabled);
		buttons[Buttons::BT_POINT_SM]->set_active(enabled);
		buttons[Buttons::BT_POT_SM]->set_active(enabled);
		buttons[Buttons::BT_UPGRADE_SM]->set_active(enabled);
		buttons[Buttons::BT_APPRAISE_SM]->set_active(enabled);
		buttons[Buttons::BT_EXTRACT_SM]->set_active(enabled);
		buttons[Buttons::BT_DISASSEMBLE_SM]->set_active(enabled);
		buttons[Buttons::BT_TOAD_SM]->set_active(enabled);
		buttons[Buttons::BT_CASHSHOP]->set_active(enabled);

		set_sort(sort_enabled);
		load_icons();
	}

	void UIItemInventory::ItemIcon::set_count(int16_t c)
	{
		count = c;
	}

	Icon::IconType UIItemInventory::ItemIcon::get_type()
	{
		return Icon::IconType::ITEM;
	}

	UIItemInventory::ItemIcon::ItemIcon(const UIItemInventory& parent, InventoryType::Id st, Equipslot::Id eqs, int16_t s, int32_t iid, int16_t c, bool u, bool cash) : parent(parent)
	{
		sourcetab = st;
		eqsource = eqs;
		source = s;
		item_id = iid;
		count = c;
		untradable = u;
		cashitem = cash;
	}

	void UIItemInventory::ItemIcon::drop_on_stage() const
	{
		constexpr char* dropmessage = "How many will you drop?";
		constexpr char* untradablemessage = "This item can't be taken back once thrown away.\\nWill you still drop it?";
		constexpr char* cashmessage = "You can't drop this item.";

		if (cashitem)
		{
			UI::get().emplace<UIOk>(cashmessage, [](bool) {});
		}
		else
		{
			if (untradable)
			{
				auto onok = [&, dropmessage](bool ok)
				{
					if (ok)
					{
						if (count <= 1)
						{
							MoveItemPacket(sourcetab, source, 0, 1).dispatch();
						}
						else
						{
							auto onenter = [&](int32_t qty)
							{
								MoveItemPacket(sourcetab, source, 0, qty).dispatch();
							};

							UI::get().emplace<UIEnterNumber>(dropmessage, onenter, count, count);
						}
					}
				};

				UI::get().emplace<UIYesNo>(untradablemessage, onok);
			}
			else
			{
				if (count <= 1)
				{
					MoveItemPacket(sourcetab, source, 0, 1).dispatch();
				}
				else
				{
					auto onenter = [&](int32_t qty)
					{
						MoveItemPacket(sourcetab, source, 0, qty).dispatch();
					};

					UI::get().emplace<UIEnterNumber>(dropmessage, onenter, count, count);
				}
			}
		}
	}

	void UIItemInventory::ItemIcon::drop_on_equips(Equipslot::Id eqslot) const
	{
		switch (sourcetab)
		{
		case InventoryType::Id::EQUIP:
			if (eqsource == eqslot)
				if (parent.can_wear_equip(source))
					EquipItemPacket(source, eqslot).dispatch();

			Sound(Sound::Name::DRAGEND).play();

			break;
		case InventoryType::Id::USE:
			ScrollEquipPacket(source, eqslot).dispatch();
			break;
		}
	}

	bool UIItemInventory::ItemIcon::drop_on_items(InventoryType::Id tab, Equipslot::Id, int16_t slot, bool) const
	{
		if (tab != sourcetab || slot == source)
			return true;

		MoveItemPacket(tab, source, slot, 1).dispatch();

		return true;
	}

	void UIItemInventory::ItemIcon::drop_on_bindings(Point<int16_t> cursorposition, bool remove) const
	{
		if (sourcetab == InventoryType::Id::USE || sourcetab == InventoryType::Id::SETUP)
		{
			auto keyconfig = UI::get().get_element<UIKeyConfig>();
			Keyboard::Mapping mapping = Keyboard::Mapping(KeyType::ITEM, item_id);

			if (remove)
				keyconfig->unstage_mapping(mapping);
			else
				keyconfig->stage_mapping(cursorposition, mapping);
		}
	}
}