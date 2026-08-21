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

#include "../Components/AreaButton.h"
#include "../Components/Textfield.h"

namespace ms
{
	class UIWorldMap : public UIDragElement<PosMAP>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::WORLDMAP;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIWorldMap();

		void draw(float inter) const override;
		void update() override;

		void toggle_active() override;

		void remove_cursor() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursor_pos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		// Fill a screen of this size instead of sitting in its own frame.
		//
		// The handheld's lower panel shows this map full-bleed - no window
		// border, no separate search panel down the side, the map itself scaled
		// up until it covers the screen. Opened over the game on the main screen
		// it is left exactly as it was, which is why this is a mode rather than a
		// change to the window.
		void set_panel(Point<int16_t> screen);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

		// Nothing to drag when this fills the panel. The window is pinned there
		// and put back every frame, so a drag moved it and the next frame moved
		// it back - while the hit-test read the position it was being dragged
		// to. That is the pointer wandering and losing track of what is under
		// it.
		bool indragrange(Point<int16_t> cursorpos) const override;

	private:
		static constexpr uint8_t MAPSPOT_TYPE_MAX = 4u;

		void set_search(bool enable);

		// Work out the scale that covers the panel and put the controls in a row
		// inside the top of the map. Re-run whenever the map image changes, since
		// each region's picture is a different size.
		void layout_panel();

		// An offset measured on the map picture, in panel terms. Everything the
		// map places - spots, link areas, the path overlay - is measured against
		// the picture, so stretching the picture has to stretch these by the
		// same amount or they sit where the map used to be.
		Point<int16_t> scaled(Point<int16_t> offset) const;

		// A point on the map image, in screen terms. Spots are given relative to
		// the image's middle, so scaling the map has to scale them too.
		Point<int16_t> map_point(Point<int16_t> spot) const;

		// A picture laid over the whole map - a region highlight or a path -
		// placed by its own origin and stretched with the map beneath it.
		void draw_overlay(const Texture& overlay) const;

		void update_world(std::string parent_map);

		enum Buttons
		{
			BT_CLOSE,
			BT_SEARCH,
			BT_AUTOFLY,
			BT_NAVIREG,
			BT_ALLSEARCH,
			BT_SEARCH_CLOSE,
			BT_BACK,
			BT_LINK0,
			BT_LINK1,
			BT_LINK2,
			BT_LINK3,
			BT_LINK4,
			BT_LINK5,
			BT_LINK6,
			BT_LINK7,
			BT_LINK8,
			BT_LINK9
		};

		struct MapSpot
		{
			std::string description;
			Texture path;
			std::string title;
			uint8_t type;
			Texture marker;
			bool bolded;
			std::vector<int32_t> map_ids;
		};

		bool search;
		bool show_path_img;

		int32_t mapid;

		std::string parent_map;
		std::string user_map;

		Texture search_background;
		Texture search_notice;
		Texture base_img;
		Texture path_img;

		Animation cur_pos;
		Animation npc_pos[MAPSPOT_TYPE_MAX];

		Textfield search_text;

		std::map<uint16_t, Texture> link_images;
		std::map<uint16_t, std::string> link_maps;

		std::vector<std::pair<Point<int16_t>, MapSpot>> map_spots;

		Point<int16_t> bg_dimensions;
		Point<int16_t> bg_search_dimensions;
		Point<int16_t> background_dimensions;
		Point<int16_t> base_position;

		// Set when this copy is the one on the lower panel.
		bool panel;
		Point<int16_t> panel_screen;
		Point<int16_t> panel_map_size;
		float panel_scale_x;
		float panel_scale_y;

		// Where the search box sits, so a plate can be drawn behind it.
		Rectangle<int16_t> search_box;
	};
}