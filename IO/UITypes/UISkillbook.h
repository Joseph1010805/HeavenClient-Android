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
#include "../UIDragElement.h"

#include "../Components/Slider.h"
#include "../Components/Charset.h"
#include "../Components/StatefulIcon.h"
#include "../Character/CharStats.h"
#include "../Character/Skillbook.h"
#include "../Graphics/Text.h"

namespace ms
{
	class UISkillbook : public UIDragElement<PosSKILL>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::SKILLBOOK;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UISkillbook(const CharStats& stats, const Skillbook& skillbook);

		// Show this copy on the lower panel: pinned, with the window's own
		// close box and drag bar taken away.
		void set_panel(Point<int16_t> screen);

		void draw(float alpha) const override;

		void toggle_active() override;
		void doubleclick(Point<int16_t> cursorpos) override;
		void remove_cursor() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		// Scroll wheel, and the thumbstick that now behaves like one.
		//
		// Slider::send_scroll has always existed; this window simply never
		// offered it anything, so the bar could only be dragged.
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		void update_stat(Maplestat::Id stat, int16_t value);
		void update_skills(int32_t skill_id);
		bool is_skillpoint_enabled();

	protected:
		bool indragrange(Point<int16_t> cursorpos) const override;

		bool panel = false;
		static constexpr float PANEL_FADE = 0.6f;

		// THE PANEL PAGE, BUILT LIKE THE EQUIPMENT PAGE.
		//
		// Same numbers as UIEquipInventory deliberately - a 32x32 box on a
		// 52x50 pitch, a grid starting at 46, a tab row above it and one wide
		// action bar along the bottom. Copied rather than derived so the two
		// pages can be compared by reading them side by side.
		static constexpr int16_t P_COLS = 6;
		static constexpr int16_t P_CELL_W = 52;
		static constexpr int16_t P_CELL_H = 50;
		static constexpr int16_t P_GRID_TOP = 46;
		static constexpr int16_t P_TAB_TOP = 3;
		static constexpr int16_t P_TAB_H = 22;
		static constexpr int16_t P_ACTION_H = 28;

		// How far in from either edge anything on this page starts.
		//
		// The HP and MP gauges run the FULL HEIGHT of both edges of the panel
		// and are drawn after the page, so anything within 12 pixels of a side
		// is painted over. The stat sheet above lost the first letter of every
		// row this way - "STR" read as "TR".
		static constexpr int16_t P_EDGE = 16;

		Point<int16_t> panel_screen = Point<int16_t>(344, 300);

		// How tall this page is with the skills it is currently showing.
		//
		// THE PAGE DOES NOT OWN THE PANEL. It is the lower half of the
		// character column, under the stat sheet, and that column scrolls -
		// so everything is measured from this window's own top and its own
		// height, never from the panel's. Laying the action bar out against
		// the panel's 300 put it below the stat sheet's height as well, which
		// is exactly how far off the bottom of the screen SPEND ended up.
		int16_t panel_height() const;
		void relayout_panel();

		void draw_panel(float alpha) const;

	public:
		// WHAT IS PICKED, AS SOMETHING A HOTKEY CAN HOLD.
		//
		// This did not exist, so the panel asked the skill page what was
		// selected, got NONE, and carried nothing to the hotkeys. The item
		// inventory has had one all along, which is why potions ALMOST
		// worked and skills did not work at all.
		Keyboard::Mapping selected_mapping() const override;

	private:

		// Hit tests, in panel coordinates. -1 for nothing.
		int16_t panel_tab_at(Point<int16_t> at) const;
		int16_t panel_cell_at(Point<int16_t> at) const;

		Rectangle<int16_t> panel_tab_box(uint16_t tabid) const;
		Rectangle<int16_t> panel_cell_box(size_t index) const;
		Rectangle<int16_t> panel_action_box() const;
		Rectangle<int16_t> panel_minus_box() const;
		Rectangle<int16_t> panel_plus_box() const;

		// Handles a press on the panel. True when it was ours, so the book's
		// own borrowed buttons never see it.
		bool panel_pressed(Point<int16_t> at);

		// How many points the SPEND button will send. The old window asked
		// this with 200 pixels of borrowed artwork that does not fit here.
		int16_t panel_spend = 1;

		// Which skill is picked out, as an index into `skills`. -1 for none.
		int16_t panel_selected = -1;

		mutable Text panel_tab_text;
		mutable Text panel_level_text;
		mutable Text panel_name_text;
		mutable Text panel_action_text;


		Button::State button_pressed(uint16_t id) override;

	private:
		class SkillIcon : public StatefulIcon::Type
		{
		public:
			SkillIcon(int32_t skill_id);

			void drop_on_stage() const override {}
			void drop_on_equips(Equipslot::Id) const override {}
			bool drop_on_items(InventoryType::Id, Equipslot::Id, int16_t, bool) const override { return true; }
			void drop_on_bindings(Point<int16_t> cursorposition, bool remove) const override;
			void set_count(int16_t) override {}
			void set_state(StatefulIcon::State) override {}
			Icon::IconType get_type() override;
			Keyboard::Mapping get_mapping() const override { return Keyboard::Mapping(KeyType::Id::SKILL, skill_id); }

		private:
			int32_t skill_id;
		};

		class SkillDisplayMeta
		{
		public:
			SkillDisplayMeta(int32_t id, int32_t level);

			void draw(const DrawArgument& args) const;

			int32_t get_id() const;
			int32_t get_level() const;
			StatefulIcon* get_icon() const;

		private:
			int32_t id;
			int32_t level;
			std::unique_ptr<StatefulIcon> icon;
			Text name_text;
			Text level_text;
		};

		void change_job(uint16_t id);
		void change_sp();
		void change_tab(uint16_t new_tab);
		void change_offset(uint16_t new_offset);

		void show_skill(int32_t skill_id);
		void clear_tooltip();

		bool can_raise(int32_t skill_id) const;
		void send_spup(uint16_t row);
		void spend_sp(int32_t skill_id);

		Job::Level joblevel_by_tab(uint16_t tab) const;

		// The spare points for whichever book is open.
		//
		// This used to be read back out of the label with std::stoi, which
		// throws on an empty string - and the label IS empty until change_sp()
		// has run at least once. Every button in the window parsed it before
		// doing anything, the Close button included, so the window could take
		// the whole game down rather than shut.
		int16_t spare_sp() const;
		const UISkillbook::SkillDisplayMeta* skill_by_position(Point<int16_t> cursorpos) const;

		void close();
		bool check_required(int32_t id) const;

		void set_macro(bool enabled);
		void set_skillpoint(bool enabled);

		enum Buttons : uint16_t
		{
			BT_CLOSE,
			BT_HYPER,
			BT_GUILDSKILL,
			BT_RIDE,
			BT_MACRO,
			BT_MACRO_OK,
			BT_CANCLE,
			BT_OKAY,
			BT_SPDOWN,
			BT_SPMAX,
			BT_SPUP,
			BT_TAB0,
			BT_TAB1,
			BT_TAB2,
			BT_TAB3,
			BT_TAB4,
			BT_SPUP0,
			BT_SPUP1,
			BT_SPUP2,
			BT_SPUP3,
			BT_SPUP4,
			BT_SPUP5,
			BT_SPUP6,
			BT_SPUP7,
			BT_SPUP8,
			BT_SPUP9,
			BT_SPUP10,
			BT_SPUP11
		};

		// One skill per row, scrolled. Upstream lays these out in two columns
		// of six against a much larger window - which is why half the skills
		// had their spend-point arrow at x 278, outside a window 174 wide, and
		// could not be clicked at all.
		//
		// Measured from the artwork rather than carried over: the window is
		// 174x299, its inner list panel is 160x224 at (7,47), and a row is
		// 140x35. Six rows of 37 fill that panel with a pixel to spare, and
		// 140 centred in 160 puts the row at x 17.
		static constexpr int16_t ROWS = 6;
		static constexpr int16_t ROW_HEIGHT = 37;
		static constexpr Point<int16_t> SKILL_OFFSET = Point<int16_t>(17, 47);
		static constexpr int16_t LIST_HEIGHT = ROWS * ROW_HEIGHT;
		static constexpr int16_t ROW_ART_WIDTH = 140;
		static constexpr Point<int16_t> SKILL_META_OFFSET = Point<int16_t>(2, 2);
		static constexpr Point<int16_t> LINE_OFFSET = Point<int16_t>(0, 37);

		const CharStats& stats;
		const Skillbook& skillbook;

		Slider slider;
		Texture skille;
		Texture skilld;
		Texture skillb;
		Texture line;
		Texture bookicon;
		Text booktext;
		Text splabel;

		Job job;

		// Zeroed here rather than left to the constructor, which does not
		// mention either of them. change_sp() is what fills them in, and
		// anything asking before that ran would otherwise be reading whatever
		// happened to be on the stack.
		int16_t sp = 0;
		int16_t beginner_sp = 0;

		uint16_t tab;
		uint16_t skillcount;
		uint16_t offset;

		std::vector<SkillDisplayMeta> skills;
		bool grabbing;

		Point<int16_t> bg_dimensions;

		bool macro_enabled;
		Texture macro_backgrnd;
		Texture macro_backgrnd2;
		Texture macro_backgrnd3;

		bool sp_enabled;
		Texture sp_backgrnd;
		Texture sp_backgrnd2;
		Texture sp_backgrnd3;
		Charset sp_before;
		Charset sp_after;
		std::string sp_before_text;
		std::string sp_after_text;
		Text sp_used;
		Text sp_remaining;
		Text sp_name;
		Texture sp_skill;
		int32_t sp_id;
		int32_t sp_masterlevel;
	};
}