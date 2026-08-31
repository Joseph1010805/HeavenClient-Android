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

			return Cursor::State::CLICKING;
		}

		fire(slots[slot]);

		return Cursor::State::CLICKING;
	}

	UIElement::Type UIHotkeys::get_type() const
	{
		return TYPE;
	}
}
