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

#include "../Components/Slider.h"
#include "../Graphics/Text.h"
#include "../../Graphics/SpecialText.h"

namespace ms
{
	// The Item inventory.
	class UIItemInventory : public UIDragElement<PosINV>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::ITEMINVENTORY;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIItemInventory(const Inventory& inventory);

		// Show this copy on the lower panel rather than over the game.
		//
		// The wide layout is used there and nothing else changes: at 594x363 it
		// fits the panel with room to spare, and it shows every slot at once
		// instead of a column of six - which is the whole reason to give the
		// inventory a screen of its own.
		void set_panel(Point<int16_t> screen);

		void draw(float inter) const override;
		void update() override;

		void doubleclick(Point<int16_t> position) override;
		bool send_icon(const Icon& icon, Point<int16_t> position) override;
		void toggle_active() override;
		void remove_cursor() override;
		Cursor::State send_cursor(bool pressed, Point<int16_t> position) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		void modify(InventoryType::Id type, int16_t pos, int8_t mode, int16_t arg);
		void set_sort(bool enabled);
		void change_tab(InventoryType::Id type);
		void clear_new();

	protected:
		// Pinned to the panel, so there is nothing to drag - and a drag that
		// moves the window while the panel puts it back every frame is what
		// made the pointer lose track of what it was over on the map.
		bool indragrange(Point<int16_t> cursorpos) const override;


		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void show_item(int16_t slot);
		void clear_tooltip();
		void load_icons();
		void update_slot(int16_t slot);
		bool is_visible(int16_t slot) const;
		bool is_not_visible(int16_t slot) const;
		bool can_wear_equip(int16_t slot) const;
		int16_t slot_by_position(Point<int16_t> position) const;
		uint16_t button_by_tab(InventoryType::Id tab) const;
		Point<int16_t> get_slotpos(int16_t slot) const;
		Point<int16_t> get_full_slotpos(int16_t slot) const;
		Point<int16_t> get_tabpos(InventoryType::Id tab) const;
		Icon* get_icon(int16_t slot);
		void set_full(bool enabled);

		class ItemIcon : public Icon::Type
		{
		public:
			ItemIcon(const UIItemInventory& parent, InventoryType::Id sourcetab, Equipslot::Id eqsource, int16_t source, int32_t item_id, int16_t count, bool untradable, bool cashitem);

			void drop_on_stage() const override;
			void drop_on_equips(Equipslot::Id eqslot) const override;
			bool drop_on_items(InventoryType::Id tab, Equipslot::Id eqslot, int16_t slot, bool equip) const override;
			void drop_on_bindings(Point<int16_t> cursorposition, bool remove) const override;
			void set_count(int16_t count) override;
			Icon::IconType get_type() override;
			Keyboard::Mapping get_mapping() const override { return Keyboard::Mapping(KeyType::Id::ITEM, item_id); }

		private:
			InventoryType::Id sourcetab;
			Equipslot::Id eqsource;
			int16_t source;
			int32_t item_id;
			int16_t count;
			bool untradable;
			bool cashitem;
			const UIItemInventory& parent;
		};

		static constexpr uint16_t ROWS = 8;
		static constexpr uint16_t COLUMNS = 4;
		static constexpr uint16_t MAXSLOTS = ROWS * COLUMNS;
		static constexpr uint16_t MAXFULLSLOTS = COLUMNS * MAXSLOTS;
		// The panel's grid: wide and shallow, to use a screen that is wider
		// than it is tall - the narrow window's four columns leave most of it
		// empty.
		//
		// How many columns is worked out from the panel's actual width rather
		// than fixed. The panel is laid out in a space 300 high, whatever the
		// display's real pixels, so its width is not something to guess at -
		// guessing 620 when it is 344 put nine columns of the grid off the
		// sides of the screen.
		static constexpr uint16_t PANEL_ROWS = 4;
		static constexpr int16_t PANEL_SIDE = 8;

		// Where that grid starts inside the window, and how much room the tabs
		// above and the action button below need.
		static constexpr int16_t PANEL_GRID_TOP = 46;
		static constexpr int16_t PANEL_TAB_TOP = 8;
		static constexpr int16_t PANEL_ACTION_H = 26;
		static constexpr int16_t PANEL_ACTION_W = 96;

		static constexpr uint16_t ICON_WIDTH = 36;
		static constexpr uint16_t ICON_HEIGHT = 35;

		enum Buttons
		{
			BT_CLOSE,
			BT_TAB_EQUIP,
			BT_TAB_USE,
			BT_TAB_ETC,
			BT_TAB_SETUP,
			BT_TAB_CASH,
			BT_COIN,
			BT_POINT,
			BT_GATHER,
			BT_SORT,
			BT_FULL,
			BT_SMALL,
			BT_POT,
			BT_UPGRADE,
			BT_APPRAISE,
			BT_EXTRACT,
			BT_DISASSEMBLE,
			BT_TOAD,
			BT_COIN_SM,
			BT_POINT_SM,
			BT_GATHER_SM,
			BT_SORT_SM,
			BT_FULL_SM,
			BT_SMALL_SM,
			BT_POT_SM,
			BT_UPGRADE_SM,
			BT_APPRAISE_SM,
			BT_EXTRACT_SM,
			BT_DISASSEMBLE_SM,
			BT_TOAD_SM,
			BT_CASHSHOP
		};

		const Inventory& inventory;

		Animation newitemslot;
		Animation newitemtab;
		Texture projectile;
		Texture disabled;
		Text mesolabel;
		Text maplepointslabel;
		Slider slider;

		std::map<int16_t, std::unique_ptr<Icon>> icons;
		std::map<InventoryType::Id, std::pair<int16_t, int16_t>> slotrange;

		InventoryType::Id tab;
		InventoryType::Id newtab;
		int16_t newslot;
		bool ignore_tooltip;

		bool sort_enabled;
		bool full_enabled;

		// Set when this copy is the one on the lower panel.
		bool panel = false;
		Point<int16_t> panel_screen;
		int16_t panel_columns = 4;

		// Work out the wide layout: how many columns fit, where the grid
		// starts, and where the tabs and the action button sit around it.
		void layout_panel();

		// Which slot is picked, 0 for none.
		//
		// The stock window has no notion of a selected item - a press starts a
		// DRAG and a double click acts on it. Neither survives a touch screen:
		// the drag greys the slot it came from and never ends, so tapping two
		// items in turn leaves both greyed, and a tap is not a double click.
		// Here a tap picks, and the action button acts on what is picked.
		int16_t selected = 0;

		// Equip it, or use it - whichever this tab means. Shared with the
		// double click, so both do the same thing.
		void activate_slot(int16_t slot);

		// Where the action button sits, in this window's own coordinates, and
		// what it says. Empty when the tab has no action.
		Rectangle<int16_t> action_bounds() const;
		const char* action_label() const;

		Point<int16_t> grid_origin() const;

		// How the grid is arranged. The narrow window stacks four to a row and
		// scrolls; the panel is wide and short, so it lays them out across.
		int16_t columns() const;
		int16_t visible_rows() const;

		OutlinedText action_text;
		Texture backgrnd;
		Texture backgrnd2;
		Texture backgrnd3;
		Texture full_backgrnd;
		Texture full_backgrnd2;
		Texture full_backgrnd3;
		Point<int16_t> bg_dimensions;
		Point<int16_t> bg_full_dimensions;
	};
}