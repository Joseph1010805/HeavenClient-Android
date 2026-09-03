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

#include "../Graphics/Text.h"
#include "Character/CharStats.h"
#include "../Gameplay/Stage.h"

#include "../Components/MapleButton.h"
#include "../Components/MapTooltip.h"
#include "../Components/AreaButton.h"
#include "../Components/Slider.h"

namespace ms
{
	class UIMiniMap : public UIDragElement<PosMINIMAP>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MINIMAP;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMiniMap(const CharStats& stats);

		void draw(float alpha) const override;
		void update() override;

		void remove_cursor() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> pos) override;
		void send_scroll(double yoffset) override;

		// Panning, on a screen with no mouse button to hold. See
		// UIElement::send_drag.
		bool send_drag(Point<int16_t> from, Point<int16_t> to) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		// Show this copy fully opened, for the handheld's lower panel.
		//
		// The mini map remembers how the player last left it - shrunk to a corner
		// more often than not - and a whole screen given over to a shrunken map
		// would be odd. Opened over the game it still remembers, which is why
		// this is a mode rather than a change to the window.
		void set_panel(Point<int16_t> screen);

		// Place names go into THIS tooltip rather than the main UI's single
		// shared one - see UIWorldMap::set_panel_tooltip.
		void set_panel_tooltip(MapTooltip* tooltip);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		// Where place names go, when this copy is not to use the shared one.
		MapTooltip* panel_tooltip = nullptr;

		// Fill whichever tooltip this copy owns.
		void show_place(std::string name, std::string description, int32_t mapid);

		static constexpr int16_t CENTER_START_X = 64;
		static constexpr int16_t BTN_MIN_Y = 4;
		static constexpr int16_t ML_MR_Y = 17;
		static constexpr int16_t MAX_ADJ = 40;

		// How far above the middle of the panel the player rides. Applied
		// before the map is clamped to its own edges, so it can never pull the
		// view off the map - at the top or bottom of a map it simply stops
		// mattering.
		// The button says CENTRE, so centring is what it has to do.
		//
		// This used to be 34: the player rode a little above the middle,
		// which shows more of the ground ahead and was a deliberate choice.
		// It is also the reason pressing CENTRE left the dot visibly off
		// centre, and a control that does not do what it says is worse than
		// a view that shows slightly less floor. Put it back to 34 if the
		// lift is wanted more than the honesty.
		static constexpr int16_t PANEL_LIFT = 0;
		static constexpr int16_t M_START = 36;
		static constexpr int16_t LISTNPC_ITEM_HEIGHT = 17;
		static constexpr int16_t LISTNPC_ITEM_WIDTH = 140;
		static constexpr int16_t LISTNPC_TEXT_WIDTH = 114;
		static constexpr Point<int16_t> WINDOW_UL_POS = Point<int16_t>(0, 0);

		void update_buttons();
		void toggle_buttons();
		void update_text();
		void update_canvas();
		void draw_movable_markers(Point<int16_t> init_pos, float alpha) const;

		// A point on the map canvas, in screen terms. On the panel the canvas
		// is scaled to fill, so everything drawn on it scales with it.
		Point<int16_t> panel_point(Point<int16_t> spot) const;
		Point<int16_t> panel_marker(Point<int16_t> on_canvas) const;
		void layout_panel();

		// Where the map sits so the player is in the middle, clamped to
		// its edges. Changes as the player walks.
		Point<int16_t> panel_view() const;

		// DRAGGING THE MAP ABOUT.
		//
		// The panel view normally keeps the player in the middle and moves
		// the map under them, which is right while you are walking and wrong
		// the moment you want to LOOK at somewhere else. A drag adds an
		// offset to that; the CENTRE button throws the offset away.
		//
		// Panning is remembered until you ask to be centred again, rather
		// than springing back on its own: a map that pulls itself out from
		// under your thumb cannot be read.
		Point<int16_t> panel_pan;
		Point<int16_t> pan_from;
		bool panning = false;

		// Whether the view is currently anywhere other than on the player,
		// so the button can say something useful and hide when it is not
		// needed.
		bool panel_panned() const
		{
			return panel_pan.x() != 0 || panel_pan.y() != 0;
		}

		Rectangle<int16_t> panel_centre_box() const;

		mutable Text centre_label;
		void update_static_markers();
		void set_npclist_active(bool active);
		void update_dimensions();
		void update_npclist();
		void draw_npclist(Point<int16_t> minimap_dims, float alpha) const;
		void select_npclist(int16_t choice);

		enum Buttons
		{
			BT_MIN,
			BT_MAX,
			BT_SMALL,
			BT_BIG,
			BT_MAP,
			BT_NPC
		};

		enum Type
		{
			MIN,
			NORMAL,
			MAX
		};

		// Constants
		int32_t mapid;
		int8_t type;
		int8_t user_type;
		bool simpleMode;
		bool big_map;
		bool has_map;
		int16_t scale;
		nl::node Map;
		nl::node MiniMap;
		nl::node marker;
		Texture map_sprite;
		Animation player_marker;
		int16_t combined_text_width;
		int16_t middle_right_x;
		int16_t bt_min_width;
		int16_t bt_max_width;
		int16_t bt_map_width;
		std::vector<Sprite> min_sprites;
		std::vector<Sprite> normal_sprites;
		std::vector<Sprite> max_sprites;
		std::vector<std::pair<std::string, Point<int16_t>>> static_marker_info;
		int16_t map_draw_origin_x, map_draw_origin_y;

		// Set when this copy is the one on the lower panel.
		bool panel;
		Point<int16_t> panel_screen;

		// The scale for the canvas and everything drawn on it, and where the
		// scaled canvas sits.
		float panel_zoom;
		Point<int16_t> panel_size;
		bool panel_needs_layout = false;
		Point<int16_t> center_offset;
		Point<int16_t> min_dimensions;
		Point<int16_t> normal_dimensions;
		Point<int16_t> max_dimensions;
		Text combined_text;
		Text region_text;
		Text town_text;

		bool listNpc_enabled;
		nl::node listNpc;
		std::vector<Sprite> listNpc_sprites;
		std::vector<MapObject*> listNpc_list;
		std::vector<Text> listNpc_names;
		std::vector<std::string> listNpc_full_names;

		Point<int16_t> listNpc_dimensions;

		Slider listNpc_slider;
		int16_t listNpc_offset;
		int16_t selected;
		Animation selected_marker;

		const CharStats& stats;
	};
}