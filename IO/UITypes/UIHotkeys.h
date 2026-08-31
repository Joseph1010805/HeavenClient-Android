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

#include "../../Graphics/SpecialText.h"
#include "../../Graphics/Texture.h"

namespace ms
{
	// The lower panel's hotkey bar: skills and items you TAP to use.
	//
	// NOT the gamepad mapping. An earlier version of this page showed the
	// twelve pad buttons and bound to the keymap, which made it a second view
	// of the quickslot bar rather than something new. These are their own
	// slots: nothing else writes to them, they are not keys, and a controller
	// is not involved.
	//
	// A slot is filled from whatever the panel was carrying when you turned to
	// this page - pick a potion up on the item page, put it down here - and
	// tapping a filled slot uses it straight away.
	class UIHotkeys : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::HOTKEYS;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UIHotkeys();

		void draw(float inter) const override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		static constexpr size_t COLS = 4;
		static constexpr size_t ROWS = 3;
		static constexpr size_t COUNT = COLS * ROWS;

	private:
		// What one slot holds. Kept as the same two fields a key binding uses,
		// so the item and skill pages can hand one straight over - but stored
		// here rather than in the keymap.
		struct Slot
		{
			KeyType::Id type = KeyType::Id::NONE;
			int32_t action = 0;
		};

		int16_t cell_at(Point<int16_t> position) const;
		Point<int16_t> cell_origin(size_t slot) const;

		// Use what is in a slot. Items are consumed, skills are cast.
		void fire(const Slot& slot) const;

		void load();
		void save() const;

		// THE PANEL IS 344 x 300, NOT 620 x 540.
		//
		// 620x540 is the size of the backdrop BITMAP; the panel is laid out in
		// SecondScreen::layout_size(), which is DESIGN_HEIGHT 300 tall and as
		// wide as the panel's shape makes it - 344 on the Thor. Sizing these
		// cells against the bitmap put a 604-wide grid in a 344-wide space,
		// which is why they ran off the screen at twelve slots and again at
		// eight.
		//
		// 4 x 74 + 3 x 8 = 320 of 344, leaving 12 either side once centred.
		static constexpr int16_t CELL = 74;
		static constexpr int16_t GAP = 8;

		// No heading and no instructions. The page title above already says
		// what this is, and a line of explanatory text under it is the kind of
		// frill that makes a panel look busy without telling anyone anything
		// they cannot work out by tapping once.
		// Below the heading the panel draws for every page.
		static constexpr int16_t TOP = 30;

		Slot slots[COUNT];

		Texture cell_bg;

		mutable Text count_text;
		mutable OutlinedText index_label;
	};
}
