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

#include "../Components/EquipTooltip.h"
#include "../Components/Icon.h"
#include "../../Graphics/SpecialText.h"
#include "../Template/EnumMap.h"

#include "../Character/Inventory/Inventory.h"

namespace ms
{
	// The Equip inventory.
	class UIEquipInventory : public UIDragElement<PosEQINV>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::EQUIPINVENTORY;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIEquipInventory(const Inventory& inventory);

		// Show this copy on the lower panel rather than over the game. The
		// paper-doll layout is left exactly as it is: unlike the item grid,
		// where the slots go is what the picture MEANS.
		void set_panel(Point<int16_t> screen);

		// Which slot is picked, and where the button that acts on it sits.
		// Same idea as the item page: on a touch screen a tap PICKS and a
		// button acts, because a drag has nowhere to end.
		Equipslot::Id selected = Equipslot::Id::NONE;
		Rectangle<int16_t> action_bounds() const;
		void unequip_selected();

		OutlinedText action_text;

		void draw(float inter) const override;

		void toggle_active() override;
		void doubleclick(Point<int16_t> position) override;
		bool send_icon(const Icon& icon, Point<int16_t> position) override;
		Cursor::State send_cursor(bool pressed, Point<int16_t> position) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		void modify(int16_t pos, int8_t mode, int16_t arg);

	protected:
		// Pinned to the panel, so there is nothing to drag.
		bool indragrange(Point<int16_t> cursorpos) const override;

		bool panel = false;
		Point<int16_t> panel_screen;

		// How solid the slot squares are on the panel. The gear on top of them
		// is drawn at full strength either way.
		//
		// A fifth MORE solid than the half-strength they started at, not less -
		// the labels have to be readable against a busy picture, and at 0.4 the
		// rack was showing through them.
		static constexpr float PANEL_FADE = 0.6f;

		// The tab row on the panel: where it starts, how far apart the tabs
		// are, and how far down. UNEQUIP goes on the end of it.
		static constexpr int16_t PANEL_TAB_LEFT = 3;
		static constexpr int16_t PANEL_TAB_STEP = 40;
		static constexpr int16_t PANEL_TAB_TOP = 3;


		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void show_equip(Equipslot::Id slot);
		void clear_tooltip();
		void load_icons();
		// Refreshes BOTH boxes for a slot - the real equip and the cosmetic
		// over it. Takes the base slot; a cash slot is folded down to it.
		void update_slot(Equipslot::Id slot);
		Equipslot::Id slot_by_position(Point<int16_t> position) const;

		// On the panel the worn equipment is a grid of captioned boxes rather
		// than the character-shaped rack the window uses. The rack labels its
		// squares in the artwork, and that artwork is not drawn here - so each
		// box carries its slot's name instead, and the whole thing reads the
		// same way the bag does.
		static constexpr int16_t PANEL_COLS = 6;
		static constexpr int16_t PANEL_CELL_W = 52;
		static constexpr int16_t PANEL_CELL_H = 50;
		static constexpr int16_t PANEL_GRID_TOP = 46;

		// Where the 32x32 icon sits inside its cell.
		static constexpr int16_t PANEL_ICON_X = 10;

		// Which slots the grid shows, in order, and what to call each.
		struct PanelSlot { Equipslot::Id slot; const char* name; };
		static const PanelSlot PANEL_SLOTS[];
		static const size_t PANEL_SLOT_COUNT;

		// The CASH tab's own boxes.
		//
		// A cosmetic is worn in a slot 100 above the real one and covers it
		// without taking it off, so it needs a place of its own to be seen
		// and taken off from. Sharing the real slot's box was tried and is
		// worse: it hides the item underneath, and there is then nowhere to
		// look for something you are certain you bought.
		//
		// Only the slots that can actually take one - there is no cosmetic
		// ring or medal.
		static const PanelSlot CASH_SLOTS[];
		static const size_t CASH_SLOT_COUNT;

		// True while the CASH tab is the one showing.
		bool on_cash_tab() const;

		// The icon a box is currently showing - real gear or cosmetic,
		// following the tab. The non-const one is for picking it up.
		const Icon* shown_icon(Equipslot::Id slot) const;
		Icon* shown_icon(Equipslot::Id slot);

		// Lays the grid out and puts every slot's position in iconpositions,
		// which is what slot_by_position already reads - so hit-testing
		// follows the boxes without being told about them separately.
		void build_panel_grid();

		mutable Text panel_slot_names[Equipslot::Id::LENGTH];
		void change_tab(uint16_t tabid);
		// The slot a box acts on: the cosmetic one while the CASH tab is
		// showing, the real one otherwise.
		int16_t worn_slot(Equipslot::Id slot) const;

		class EquipIcon : public Icon::Type
		{
		public:
			EquipIcon(int16_t source);

			void drop_on_stage() const override;
			void drop_on_equips(Equipslot::Id slot) const override;
			bool drop_on_items(InventoryType::Id tab, Equipslot::Id eqslot, int16_t slot, bool equip) const override;
			void drop_on_bindings(Point<int16_t>, bool) const override {}
			void set_count(int16_t) override {}
			Icon::IconType get_type() override;

		private:
			int16_t source;
		};

		enum Buttons : uint16_t
		{
			BT_TAB0,
			BT_TAB1,
			BT_TAB2,
			BT_TAB3,
			BT_TABE,
			BT_CLOSE,
			BT_SLOT,
			BT_EFFECT,
			BT_SALON,
			BT_CONSUMESETTING,
			BT_EXCEPTION,
			BT_SHOP
		};

		const Inventory& inventory;

		EnumMap<Equipslot::Id, Point<int16_t>> iconpositions;
		EnumMap<Equipslot::Id, std::unique_ptr<Icon>> icons;

		// The cosmetics, keyed by the BASE slot they cover - a cash hat is
		// under HAT here, not under 101. `EnumMap` is an array sized by the
		// enum, so 101 has no home in one; folding to the base keeps both
		// sets in the containers that already exist.
		EnumMap<Equipslot::Id, Point<int16_t>> cash_iconpositions;
		EnumMap<Equipslot::Id, std::unique_ptr<Icon>> cash_icons;

		uint16_t tab;
		std::string tab_source[Buttons::BT_TABE];
		Texture tabbar;
		Texture background[Buttons::BT_TABE];
		Texture disabled;
		Texture disabled2;
		std::vector<Texture> Slots[Buttons::BT_TABE];

		bool hasPendantSlot;
		bool hasPocketSlot;
	};
}