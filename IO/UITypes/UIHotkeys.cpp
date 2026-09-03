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
#include "UIHotkeys.h"
#include "UIKeyConfig.h"

#include "../../Net/Packets/InventoryPackets.h"

#include "../UI.h"
#include "../SecondScreen.h"

#include "../../Configuration.h"
#include "../../Data/ItemData.h"
#include "../../Data/SkillData.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/Geometry.h"

#include <nlnx/nx.hpp>

#include <sstream>

namespace ms
{
	UIHotkeys::UIHotkeys() : UIElement(
		Point<int16_t>(0, 0),
		// The dimension is the CONTENT, with no trailing gap - a spare gap on
		// the right made the box wider than what was drawn in it, and the
		// panel centres the box, so the grid sat left of centre by half a gap.
		Point<int16_t>(COLS * CELL + (COLS - 1) * GAP,
			TOP + ROWS * CELL + (ROWS - 1) * GAP))
	{
		cell_bg = nl::nx::ui["StatusBar3.img"]["QuickSlot"]["backgrnd"];

		count_text = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::WHITE);
		index_label = OutlinedText(Text::Font::A11B, Text::Alignment::LEFT,
			Color::Name::WHITE, Color::Name::BLACK);

		load();
	}

	void UIHotkeys::load()
	{
		// "type:action,type:action,..." - see Configuration::HotkeySlots.
		std::string packed = Setting<HotkeySlots>::get().load();

		std::stringstream stream(packed);
		std::string field;

		for (size_t i = 0; i < COUNT && std::getline(stream, field, ','); i++)
		{
			size_t colon = field.find(':');

			if (colon == std::string::npos)
				continue;

			int32_t type = string_conversion::or_zero<int32_t>(field.substr(0, colon));
			int32_t action = string_conversion::or_zero<int32_t>(field.substr(colon + 1));

			if (type <= 0)
				continue;

			slots[i].type = static_cast<KeyType::Id>(type);
			slots[i].action = action;
		}
	}

	void UIHotkeys::save() const
	{
		std::string packed;

		for (size_t i = 0; i < COUNT; i++)
		{
			if (i > 0)
				packed += ',';

			packed += std::to_string(static_cast<int32_t>(slots[i].type))
				+ ':' + std::to_string(slots[i].action);
		}

		Setting<HotkeySlots>::get().save(packed);

		// AND WRITE THE FILE, NOW.
		//
		// Setting::save only changes the value in memory. The file is written
		// by Configuration::save, which the Configuration destructor calls -
		// and that destructor runs on a clean shutdown, which is not how a
		// game on a handheld ever ends. Android kills it from outside, so
		// nothing was ever written and every hotkey placed was gone by the
		// next launch.
		//
		// The same fault has been fixed once before, in the login screen, for
		// the saved account name. See UILogin::login.
		Configuration::get().save();
	}

	Point<int16_t> UIHotkeys::cell_origin(size_t slot) const
	{
		return Point<int16_t>(
			static_cast<int16_t>((slot % COLS) * (CELL + GAP)),
			static_cast<int16_t>(TOP + (slot / COLS) * (CELL + GAP)));
	}

	void UIHotkeys::draw(float inter) const
	{
		UIElement::draw(inter);

		const Inventory& inventory = Stage::get().get_player().get_inventory();

		for (size_t i = 0; i < COUNT; i++)
		{
			Point<int16_t> at = position + cell_origin(i);

			if (cell_bg.is_valid())
				cell_bg.draw(DrawArgument(at, at, Point<int16_t>(CELL, CELL),
					1.0f, 1.0f, 1.0f, 0.0f));

			const Slot& slot = slots[i];
			Texture icon;

			if (slot.type == KeyType::Id::SKILL)
				icon = SkillData::get(slot.action).get_icon(SkillData::Icon::NORMAL);
			else if (slot.type == KeyType::Id::ITEM)
				icon = ItemData::get(slot.action).get_icon(false);
			else if (slot.type != KeyType::Id::NONE)
				// Everything else is a bound ACTION, MENU or FACE, and the
				// key config's icon strip has a picture for each. A slot
				// holding Jump used to be an empty square.
				icon = UIKeyConfig::action_icon(
					static_cast<KeyAction::Id>(slot.action));

			if (icon.is_valid())
			{
				int16_t pad = 14;
				Point<int16_t> ic = at + Point<int16_t>(pad, pad);

				icon.draw(DrawArgument(ic, ic,
					Point<int16_t>(CELL - pad * 2, CELL - pad * 2),
					1.0f, 1.0f, 1.0f, 0.0f));
			}

			// How many are left, for a consumable. Nothing is more annoying
			// than a potion button that turns out to be empty.
			if (slot.type == KeyType::Id::ITEM)
			{
				int16_t held = inventory.get_total_item_count(slot.action);

				count_text.change_text(std::to_string(held));
				count_text.draw(at + Point<int16_t>(CELL - 5, CELL - 20));
			}

			index_label.change_text(std::to_string(i + 1));
			index_label.draw(at + Point<int16_t>(5, 2));
		}
	}

	int16_t UIHotkeys::cell_at(Point<int16_t> point) const
	{
		for (size_t i = 0; i < COUNT; i++)
		{
			Point<int16_t> at = cell_origin(i);

			if (point.x() >= at.x() && point.x() < at.x() + CELL
				&& point.y() >= at.y() && point.y() < at.y() + CELL)
				return static_cast<int16_t>(i);
		}

		return -1;
	}

	void UIHotkeys::fire(const Slot& slot) const
	{
		if (slot.type == KeyType::Id::NONE)
			return;

		// AN EQUIP IS WORN AND TAKEN OFF, NOT "USED".
		//
		// Stage::send_key knows how to consume a potion and cast a skill, and
		// there is nothing sensible for it to do with a hat. So a slot
		// holding a piece of equipment toggles it: off if it is on, on if it
		// is in the bag. That is the only thing anybody could mean by putting
		// a weapon on a button.
		//
		// Item ids beginning with 1 are equipment - the same test the
		// inventory uses to decide which tab something belongs in.
		bool wearable = (slot.type == KeyType::Id::ITEM
			|| slot.type == KeyType::Id::CASH)
			&& slot.action / 1000000 == 1;

		if (wearable)
		{
			const Inventory& bag = Stage::get().get_player().get_inventory();

			// ON already? Take it off, into the first free bag slot.
			for (int16_t worn = 1; worn < 120; worn++)
			{
				if (bag.get_item_id(InventoryType::Id::EQUIPPED, worn) != slot.action)
					continue;

				if (int16_t free = bag.find_free_slot(InventoryType::Id::EQUIP))
					UnequipItemPacket(worn, free).dispatch();

				return;
			}

			// Otherwise find it in the bag and put it on.
			for (int16_t held = 1; held <= bag.get_slotmax(InventoryType::Id::EQUIP); held++)
			{
				if (bag.get_item_id(InventoryType::Id::EQUIP, held) != slot.action)
					continue;

				EquipItemPacket(held, bag.find_equipslot(slot.action)).dispatch();

				return;
			}

			// Neither worn nor carried - it has been dropped or sold, and the
			// slot is pointing at nothing. Silence is right: a message every
			// time a stale button is pressed would be worse than the button.
			return;
		}

		// Stage already knows how to act on a bound thing - a skill goes to
		// combat, an item to the player. Going through it means a hotkey and a
		// key press take exactly the same path.
		Stage::get().send_key(slot.type, slot.action, true);
	}

	Cursor::State UIHotkeys::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		int16_t index = cell_at(cursorpos - position);

		if (index < 0)
			return Cursor::State::IDLE;

		if (!clicked)
			return Cursor::State::CANCLICK;

		size_t slot = static_cast<size_t>(index);
		Keyboard::Mapping carried = SecondScreen::carried_mapping();

		// Holding something? Put it down. Otherwise use what is already there.
		//
		// Filling takes priority so a slot can be replaced without having to
		// empty it first - and because tapping while carrying something is
		// unambiguously an attempt to place it.
		if (carried.type != KeyType::Id::NONE)
		{
			slots[slot].type = carried.type;
			slots[slot].action = carried.action;

			save();

			// AND YOU ARE NO LONGER HOLDING IT.
			//
			// Without this the carry outlives the placement, so the next tap
			// on any slot places the same thing again instead of using what
			// is in it. The page filled up perfectly and nothing on it could
			// ever be pressed - which is exactly how it was reported.
			SecondScreen::clear_carried();

			return Cursor::State::CLICKING;
		}

		// ONE USE PER TAP. See since_fire.
		if (since_fire < COOLDOWN)
			return Cursor::State::CLICKING;

		since_fire = 0;

		fire(slots[slot]);

		return Cursor::State::CLICKING;
	}

	void UIHotkeys::update()
	{
		if (since_fire < COOLDOWN)
			since_fire++;

		UIElement::update();
	}

	bool UIHotkeys::clear_at(Point<int16_t> cursorpos)
	{
		int16_t index = cell_at(cursorpos - position);

		if (index < 0)
			return false;

		Slot& slot = slots[static_cast<size_t>(index)];

		if (slot.type == KeyType::Id::NONE)
			return false;

		slot.type = KeyType::Id::NONE;
		slot.action = 0;

		// Straight to the settings file, like placing one. A slot that empties
		// on screen and fills itself back in at the next login is worse than
		// one that cannot be emptied at all.
		save();

		return true;
	}

	UIElement::Type UIHotkeys::get_type() const
	{
		return TYPE;
	}
}
