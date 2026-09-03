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

		// Scroll wheel, and the thumbstick that now behaves like one.
		//
		// Slider::send_scroll has always existed; this window simply never
		// offered it anything, so the bar could only be dragged.
		void send_scroll(double yoffset) override;
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
		static constexpr int16_t PANEL_TAB_LEFT = 42;
		static constexpr int16_t PANEL_ACTION_H = 26;
		static constexpr int16_t PANEL_ACTION_W = 96;

		// HOW FAR APART THE SLOTS SIT ON THE PANEL.
		//
		// The icon artwork stays 36x35; this is the PITCH between cells. Edge
		// to edge they read as one grey mass and a thumb cannot tell which one
		// it is on - the equipment page breathes, and this makes the bag match.
		//
		// Used by the position, the hit test AND the grid origin. One number,
		// so they cannot drift apart and leave taps landing a slot away.
		// THE SAME NUMBERS THE EQUIPMENT PAGE USES.
		//
		// Copied deliberately rather than chosen to look similar: the two
		// pages sit one swipe apart and a bag whose cells are a few pixels
		// off the rack's reads as a different game. See UIEquipInventory's
		// PANEL_COLS / PANEL_CELL_W / PANEL_CELL_H / PANEL_GRID_TOP.
		//
		// 6 x 52 = 312 of the panel's 344.
		static constexpr int16_t PANEL_COLS = 6;
		static constexpr int16_t PANEL_CELL_W = 52;
		static constexpr int16_t PANEL_CELL_H = 50;

		// The BOX drawn in a cell, which is the icon's size and not the
		// cell's - equipment draws 32x32 boxes inside a 52x50 pitch, so the
		// squares have air around them instead of tiling into a sheet.
		static constexpr int16_t CELL_BOX = 32;

		// THE TABS, AS BUTTONS WITH OUR OWN ARTWORK.
		//
		// The window's own tabs are a strip of the game's UI artwork, sized
		// for a mouse and drawn in a style nothing else on this panel shares.
		// These are the same five destinations as icon buttons, hit-tested
		// here and handed to the SAME button_pressed - so the tab logic is
		// not duplicated, only the thing you press.
		// THE SAME SHAPE AS A MENU BUTTON: a faint cell, the icon inside it,
		// and the name underneath. Not a chip - the panel has one button style
		// and a row that invents a second one is the thing that looks wrong.
		static constexpr int16_t TAB_W = 60;
		static constexpr int16_t TAB_H = 48;
		static constexpr int16_t TAB_GAP = 4;

		Rectangle<int16_t> panel_tab_box(size_t index) const;
		int16_t panel_tab_at(Point<int16_t> at) const;

		mutable std::vector<Texture> tab_art;
		mutable Text tab_label;

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
		// Use something from the CASH tab. Its own opcode, and rate coupons
		// are not "used" at all - see the definition.
		void use_cash_item(int16_t slot, int32_t item_id);

		// What is picked out on the panel, so the quickslot bar on the MAIN
		// screen can bind it. Only while hosted on the panel: on the main
		// screen an item is carried to the bar by dragging, which already
		// works and needs none of this.
		Keyboard::Mapping selected_mapping() const override;

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