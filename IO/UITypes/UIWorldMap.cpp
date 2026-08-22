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
#include "UIWorldMap.h"

#include "../UI.h"

#include "../Gameplay/Stage.h"
#include "../Graphics/GraphicsGL.h"
#include "../Util/Misc.h"

#include "../IO/Components/MapleButton.h"

#include <nlnx/nx.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace ms
{
	namespace
	{
		// Every button in this artwork is 16 tall.
		constexpr int16_t BUTTON_H = 16;
		constexpr int16_t NAVI_W = 99;
		constexpr int16_t BACK_W = 61;

		// How far in from the sides the row starts and ends.
		constexpr int16_t EDGE = 6;
		constexpr int16_t GAP = 8;

		// And how far up from the bottom. As low as the row can go while every
		// button is still whole - which puts it over the border the map artwork
		// draws around itself, and that border is only decoration.
		constexpr int16_t EDGE_BOTTOM = 2;
	}

	UIWorldMap::UIWorldMap() : UIDragElement<PosMAP>(), panel(false), panel_scale_x(1.0f), panel_scale_y(1.0f)
	{
		nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];
		nl::node WorldMap = nl::nx::ui["UIWindow2.img"]["WorldMap"];
		nl::node WorldMapSearch = WorldMap["WorldMapSearch"];
		nl::node Border = WorldMap["Border"]["0"];
		nl::node backgrnd = WorldMapSearch["backgrnd"];
		nl::node MapHelper = nl::nx::map["MapHelper.img"]["worldMap"];

		cur_pos = MapHelper["curPos"];

		for (size_t i = 0; i < MAPSPOT_TYPE_MAX; i++)
			npc_pos[i] = MapHelper["npcPos" + std::to_string(i)];

		sprites.emplace_back(Border);

		search_background = backgrnd;
		search_notice = WorldMapSearch["notice"];

		bg_dimensions = Texture(Border).get_dimensions();
		bg_search_dimensions = search_background.get_dimensions();

		int16_t bg_dimension_x = bg_dimensions.x();
		background_dimensions = Point<int16_t>(bg_dimension_x, 0);

		int16_t base_x = bg_dimension_x / 2;
		int16_t base_y = bg_dimensions.y() / 2;
		base_position = Point<int16_t>(base_x, base_y + 15);

		Point<int16_t> close_dimensions = Point<int16_t>(bg_dimension_x - 22, 4);

		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(close, close_dimensions);
		buttons[Buttons::BT_SEARCH] = std::make_unique<MapleButton>(WorldMap["BtSearch"]);
		buttons[Buttons::BT_AUTOFLY] = std::make_unique<MapleButton>(WorldMap["BtAutoFly_1"]);
		buttons[Buttons::BT_NAVIREG] = std::make_unique<MapleButton>(WorldMap["BtNaviRegister"]);
		buttons[Buttons::BT_SEARCH_CLOSE] = std::make_unique<MapleButton>(close, close_dimensions + Point<int16_t>(bg_search_dimensions.x(), 0));

		// The map's own back button. Escape does this on a keyboard, and the
		// panel has no keyboard.
		buttons[Buttons::BT_BACK] = std::make_unique<MapleButton>(WorldMap["BtBefore"]);
		buttons[Buttons::BT_BACK]->set_active(false);

		buttons[Buttons::BT_ALLSEARCH] = std::make_unique<MapleButton>(WorldMapSearch["BtAllsearch"], background_dimensions);

		Point<int16_t> search_text_pos = Point<int16_t>(bg_dimension_x + 14, 25);
		Point<int16_t> search_box_dim = Point<int16_t>(83, 15);
		Rectangle<int16_t> search_text_dim = Rectangle<int16_t>(search_text_pos, search_text_pos + search_box_dim);

		search_text = Textfield(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, search_text_dim, 8);

		set_search(true);

		dragarea = Point<int16_t>(bg_dimension_x, 20);
	}

	void UIWorldMap::set_panel(Point<int16_t> screen)
	{
		panel = true;
		panel_screen = screen;

		// The side panel is what the extra width was for, and it is gone here.
		search = false;
		dimension = screen;

		// The results list and the frame's own close buttons have nothing to
		// close or sit in any more.
		buttons[Buttons::BT_CLOSE]->set_active(false);
		buttons[Buttons::BT_SEARCH_CLOSE]->set_active(false);

		layout_panel();
	}

	void UIWorldMap::set_panel_tooltip(MapTooltip* tooltip)
	{
		panel_tooltip = tooltip;
	}

	void UIWorldMap::layout_panel()
	{
		if (!panel)
			return;

		Point<int16_t> image = base_img.get_dimensions();

		if (image.x() > 0 && image.y() > 0)
		{
			// Stretched to the panel exactly, both ways. Fitting the width left
			// a band of backdrop above and below; covering cut the map off east
			// and west, where a world map's content is. Filling it outright
			// changes the proportions a little, which is the least of the three
			// costs and the one that was asked for.
			panel_scale_x = static_cast<float>(panel_screen.x()) / image.x();
			panel_scale_y = static_cast<float>(panel_screen.y()) / image.y();

			panel_map_size = panel_screen;

			// Spots are given from the middle of the picture, so this has to
			// stay the middle of it.
			base_position = Point<int16_t>(panel_screen.x() / 2, panel_screen.y() / 2);
		}

		// The controls sit along the BOTTOM now, in the corners, where they are
		// out of the map rather than over it.
		//
		// A button is drawn at its position MINUS its artwork's origin, and
		// this artwork carries big negative origins - BtNaviRegister's is
		// -365,-25 - which is how it sat in the frame that is no longer drawn.
		// So each position has that origin taken back off, or the button lands
		// hundreds of pixels away and runs off the panel.
		//
		// Sitting them right on the bottom edge means they overlap the border
		// the map artwork draws around itself. That is the better trade: the
		// border is decoration, and a control that is half off the screen is
		// not a control.
		int16_t bottom = panel_screen.y() - BUTTON_H - EDGE_BOTTOM;

		// Navigation and auto-pilot from the left, back at the right. The
		// middle of this row is left empty on purpose - the panel's page dots
		// sit there, and they belong to the panel rather than to the map, so
		// the map's own controls keep out of their way.
		int16_t x = EDGE;

		buttons[Buttons::BT_NAVIREG]->set_position(Point<int16_t>(x - 365, bottom - 25));
		buttons[Buttons::BT_NAVIREG]->set_active(true);

		x += NAVI_W + GAP;

		buttons[Buttons::BT_AUTOFLY]->set_position(Point<int16_t>(x - 468, bottom - 25));
		buttons[Buttons::BT_AUTOFLY]->set_active(true);

		// Back at the far right, and only when there is somewhere to go back
		// TO - on the top-level world map there is not.
		buttons[Buttons::BT_BACK]->set_position(Point<int16_t>(
			panel_screen.x() - EDGE - BACK_W - 515, bottom - 510));

		buttons[Buttons::BT_BACK]->set_active(!parent_map.empty());

		// Search is off for now. Tapping into the box brought the keyboard up
		// over the panel with no way back out of it, which is worse than not
		// having search at all until it works properly.
		buttons[Buttons::BT_SEARCH]->set_active(false);
		buttons[Buttons::BT_ALLSEARCH]->set_active(false);

		search_box = Rectangle<int16_t>(Point<int16_t>(0, 0), Point<int16_t>(0, 0));
		search_text.set_state(Textfield::State::DISABLED);
	}

	Point<int16_t> UIWorldMap::scaled(Point<int16_t> offset) const
	{
		if (!panel)
			return offset;

		return Point<int16_t>(
			static_cast<int16_t>(offset.x() * panel_scale_x),
			static_cast<int16_t>(offset.y() * panel_scale_y));
	}

	Point<int16_t> UIWorldMap::map_point(Point<int16_t> spot) const
	{
		return position + base_position + scaled(spot);
	}

	void UIWorldMap::draw_overlay(const Texture& overlay) const
	{
		Point<int16_t> origin = overlay.get_origin();

		if (!panel)
		{
			overlay.draw(map_point(Point<int16_t>(0, 0)));

			return;
		}

		// The overlay is measured against the unstretched picture, so it has to
		// be stretched by the same amount and put back where the piece of map
		// it belongs to has moved to. Drawn at its natural size it covered the
		// wrong part of the map - which is what made the highlight land away
		// from the place under the finger.
		//
		// A texture draws at its position MINUS its origin, so the origin goes
		// back on to put the corner where it is wanted.
		Point<int16_t> corner = map_point(Point<int16_t>(-origin.x(), -origin.y())) + origin;

		overlay.draw(DrawArgument(corner, corner, scaled(overlay.get_dimensions()), 1.0f, 1.0f, 1.0f, 0.0f));
	}

	void UIWorldMap::draw(float alpha) const
	{
		// The frame is what makes the white border, and on the panel the map
		// is meant to reach every edge.
		if (!panel)
			UIElement::draw_sprites(alpha);

		if (search)
		{
			search_background.draw(position + background_dimensions);
			search_notice.draw(position + background_dimensions);
			search_text.draw(position + Point<int16_t>(1, -5));
		}

		if (panel)
		{
			// Scaled up to cover the screen. An icon is placed by its origin,
			// so the origin is added back to put the corner where it is wanted.
			Point<int16_t> topleft = position + base_position
				- Point<int16_t>(panel_map_size.x() / 2, panel_map_size.y() / 2)
				+ base_img.get_origin();

			base_img.draw(DrawArgument(topleft, topleft, panel_map_size, 1.0f, 1.0f, 1.0f, 0.0f));
		}
		else
		{
			base_img.draw(position + base_position);
		}

		if (link_images.size() > 0)
		{
			for (auto& iter : buttons)
			{
				if (const auto button = iter.second.get())
				{
					if (iter.first >= Buttons::BT_LINK0 && button->get_state() == Button::State::MOUSEOVER)
					{
						if (link_images.find(iter.first) != link_images.end())
						{
							draw_overlay(link_images.at(iter.first));
							break;
						}
					}
				}
			}
		}

		if (show_path_img)
			draw_overlay(path_img);

		for (auto spot : map_spots)
			spot.second.marker.draw(map_point(spot.first));

		bool found = false;

		if (!found)
		{
			for (auto spot : map_spots)
			{
				for (auto map_id : spot.second.map_ids)
				{
					if (map_id == mapid)
					{
						found = true;
						npc_pos[spot.second.type].draw(map_point(spot.first), alpha);
						cur_pos.draw(map_point(spot.first), alpha);
						break;
					}
				}

				if (found)
					break;
			}
		}

		UIElement::draw_buttons(alpha);
	}

	void UIWorldMap::update()
	{
		int32_t mid = Stage::get().get_mapid();

		if (mid != mapid)
		{
			mapid = mid;
			auto prefix = mapid / 10000000;
			auto parent_map = "WorldMap0" + std::to_string(prefix);
			user_map = parent_map;

			update_world(parent_map);
		}

		if (search)
			search_text.update(position);

		for (size_t i = 0; i < MAPSPOT_TYPE_MAX; i++)
			npc_pos[i].update(1);

		cur_pos.update();

		UIElement::update();
	}

	void UIWorldMap::toggle_active()
	{
		UIElement::toggle_active();

		if (!active)
		{
			set_search(true);
			update_world(user_map);
		}
	}

	void UIWorldMap::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			if (search)
			{
				set_search(false);
			}
			else
			{
				if (parent_map == "")
				{
					toggle_active();

					update_world(user_map);
				}
				else
				{
					Sound(Sound::Name::SELECTMAP).play();

					update_world(parent_map);
				}
			}
		}
	}

	UIElement::Type UIWorldMap::get_type() const
	{
		return TYPE;
	}

	Button::State UIWorldMap::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::BT_CLOSE:
			deactivate();
			break;
		case Buttons::BT_SEARCH:
			set_search(!search);
			break;
		case Buttons::BT_SEARCH_CLOSE:
			set_search(false);
			break;
		case Buttons::BT_BACK:
			// The same step Escape takes: up to the region this one sits in.
			if (!parent_map.empty())
			{
				Sound(Sound::Name::SELECTMAP).play();

				update_world(parent_map);
			}

			return Button::State::NORMAL;
		default:
			break;
		}

		if (buttonid >= Buttons::BT_LINK0)
		{
			update_world(link_maps[buttonid]);

			return Button::State::IDENTITY;
		}

		return Button::State::NORMAL;
	}

	bool UIWorldMap::indragrange(Point<int16_t> cursorpos) const
	{
		if (panel)
			return false;

		return UIDragElement::indragrange(cursorpos);
	}

	void UIWorldMap::remove_cursor()
	{
		UIDragElement::remove_cursor();

		if (panel_tooltip)
			panel_tooltip->reset();
		else
			UI::get().clear_tooltip(Tooltip::Parent::WORLDMAP);

		show_path_img = false;
	}

	Cursor::State UIWorldMap::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (Cursor::State new_state = search_text.send_cursor(cursorpos, clicked))
			return new_state;

		show_path_img = false;

		for (auto path : map_spots)
		{
			// Where the marker was DRAWN, not where it would sit on an
			// unstretched map. These two had drifted apart on the panel, so a
			// place answered to the cursor well away from its own dot.
			Point<int16_t> p = map_point(path.first) - 10;
			Point<int16_t> d = p + path.second.marker.get_dimensions();
			Rectangle<int16_t> abs_bounds = Rectangle<int16_t>(p, d);

			if (abs_bounds.contains(cursorpos))
			{
				path_img = path.second.path;
				show_path_img = path_img.is_valid();

				if (panel_tooltip)
				{
					// Its own, so the map on the other screen keeps whatever it
					// was showing.
					panel_tooltip->set_name(Tooltip::Parent::WORLDMAP, path.second.title, path.second.bolded);
					panel_tooltip->set_desc(path.second.description);
					panel_tooltip->set_mapid(path.second.map_ids[0]);
				}
				else
				{
					UI::get().show_map(Tooltip::Parent::WORLDMAP, path.second.title, path.second.description, path.second.map_ids[0], path.second.bolded);
				}
				break;
			}
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIWorldMap::set_search(bool enable)
	{
		search = enable;

		buttons[Buttons::BT_SEARCH_CLOSE]->set_active(enable);
		buttons[Buttons::BT_ALLSEARCH]->set_active(enable);

		if (enable)
		{
			search_text.set_state(Textfield::State::NORMAL);
			dimension = bg_dimensions + Point<int16_t>(bg_search_dimensions.x(), 0);
		}
		else
		{
			search_text.set_state(Textfield::State::DISABLED);
			dimension = bg_dimensions;
		}
	}

	void UIWorldMap::update_world(std::string map)
	{
		nl::node WorldMap = nl::nx::map["WorldMap"][map + ".img"];

		if (!WorldMap)
			WorldMap = nl::nx::map["WorldMap"]["WorldMap.img"];

		base_img = WorldMap["BaseImg"][0];
		parent_map = std::string(WorldMap["info"]["parentMap"]);

		// Each region's picture is its own size, so the scale that covers the
		// panel is not the same one as the last region's.
		layout_panel();

		link_images.clear();
		link_maps.clear();

		for (auto& iter : buttons)
			if (const auto button = iter.second.get())
				if (iter.first >= Buttons::BT_LINK0)
					button->set_active(false);

		std::vector<std::pair<Texture, std::string>> links;

		for (nl::node link : WorldMap["MapLink"])
		{
			nl::node l = link["link"];

			links.emplace_back(Texture(l["linkImg"]), std::string(l["linkMap"]));
		}

		// Smallest region first.
		//
		// A region is picked out by a RECTANGLE round its highlight picture,
		// and those rectangles overlap - a small region in the middle of the
		// map sits entirely inside the box of a big one around it. Both the
		// highlight and the click take the first region in this list whose box
		// holds the cursor, so with the big one first the small one can never
		// be reached at all. That is why the middle of the map would not
		// highlight while the edges would.
		//
		// Ordering them by area means the first match is always the tightest
		// one, which is the one meant. Highlight and click read the same list,
		// so they cannot disagree about which region that is.
		std::stable_sort(links.begin(), links.end(),
			[](const std::pair<Texture, std::string>& a, const std::pair<Texture, std::string>& b)
			{
				Point<int16_t> da = a.first.get_dimensions();
				Point<int16_t> db = b.first.get_dimensions();

				return static_cast<int32_t>(da.x()) * da.y() < static_cast<int32_t>(db.x()) * db.y();
			});

		size_t i = Buttons::BT_LINK0;

		for (auto& link : links)
		{
			const Texture& link_image = link.first;

			link_images[i] = link_image;
			link_maps[i] = link.second;

			// The region's touchable area has to follow the picture. Left at the
			// unstretched size it answered where the region used to be, which
			// is why the highlight appeared away from the finger.
			Point<int16_t> origin = link_image.get_origin();

			buttons[i] = std::make_unique<AreaButton>(
				base_position + scaled(Point<int16_t>(-origin.x(), -origin.y())),
				scaled(link_image.get_dimensions()));

			buttons[i]->set_active(true);

			i++;
		}

		nl::node mapImage = nl::nx::map["MapHelper.img"]["worldMap"]["mapImage"];

		map_spots.clear();

		for (nl::node list : WorldMap["MapList"])
		{
			nl::node desc = list["desc"];
			nl::node mapNo = list["mapNo"];
			nl::node path = list["path"];
			nl::node spot = list["spot"];
			nl::node title = list["title"];
			nl::node type = list["type"];
			nl::node marker = mapImage[type];

			std::vector<int32_t> map_ids;

			for (nl::node map_no : mapNo)
				map_ids.push_back(map_no);

			if (!desc && !title)
			{
				NxHelper::Map::MapInfo map_info = NxHelper::Map::get_map_info_by_id(mapNo[0]);

				map_spots.emplace_back(std::make_pair<Point<int16_t>, MapSpot>(spot, { map_info.description, path, map_info.full_name, type, marker, true, map_ids }));
			}
			else
			{
				map_spots.emplace_back(std::make_pair<Point<int16_t>, MapSpot>(spot, { desc, path, title, type, marker, false, map_ids }));
			}
		}
	}
}