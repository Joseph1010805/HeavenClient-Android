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
#include "../Util/Misc.h"

#include "../IO/Components/MapleButton.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UIWorldMap::UIWorldMap() : UIDragElement<PosMAP>(), panel(false), panel_scale(1.0f)
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

	void UIWorldMap::layout_panel()
	{
		if (!panel)
			return;

		Point<int16_t> image = base_img.get_dimensions();

		if (image.x() > 0 && image.y() > 0)
		{
			// Cover rather than fit: the map reaches every edge and whatever
			// will not fit goes off the side, which is better than a band of
			// window behind it.
			float across = static_cast<float>(panel_screen.x()) / image.x();
			float down = static_cast<float>(panel_screen.y()) / image.y();

			panel_scale = across > down ? across : down;

			panel_map_size = Point<int16_t>(
				static_cast<int16_t>(image.x() * panel_scale),
				static_cast<int16_t>(image.y() * panel_scale));

			// Spots are given from the middle of the picture, so this has to
			// stay the middle of it.
			base_position = Point<int16_t>(panel_screen.x() / 2, panel_screen.y() / 2);
		}

		// The controls in one row just inside the top, in place of the strip
		// they had along the frame and the panel down the side.
		constexpr int16_t ROW_Y = 22;
		constexpr int16_t GAP = 8;

		int16_t x = 24;

		buttons[Buttons::BT_NAVIREG]->set_position(Point<int16_t>(x, ROW_Y));
		x += 99 + GAP;

		buttons[Buttons::BT_AUTOFLY]->set_position(Point<int16_t>(x, ROW_Y));
		x += 99 + GAP;

		buttons[Buttons::BT_SEARCH]->set_active(false);

		// The search box and its button, kept from the panel that was removed.
		Point<int16_t> box = Point<int16_t>(x, ROW_Y);
		Point<int16_t> box_size = Point<int16_t>(150, 16);

		search_text = Textfield(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK,
			Rectangle<int16_t>(box, box + box_size), 12);
		search_text.set_state(Textfield::State::NORMAL);

		x += box_size.x() + GAP;

		buttons[Buttons::BT_ALLSEARCH]->set_position(Point<int16_t>(x, ROW_Y));
		buttons[Buttons::BT_ALLSEARCH]->set_active(true);
	}

	Point<int16_t> UIWorldMap::map_point(Point<int16_t> spot) const
	{
		if (!panel)
			return spot + position + base_position;

		return position + base_position + Point<int16_t>(
			static_cast<int16_t>(spot.x() * panel_scale),
			static_cast<int16_t>(spot.y() * panel_scale));
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

			search_text.draw(position);
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
							link_images.at(iter.first).draw(map_point(Point<int16_t>(0, 0)));
							break;
						}
					}
				}
			}
		}

		if (show_path_img)
			path_img.draw(map_point(Point<int16_t>(0, 0)));

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

	void UIWorldMap::remove_cursor()
	{
		UIDragElement::remove_cursor();

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
			Point<int16_t> p = path.first + position + base_position - 10;
			Point<int16_t> d = p + path.second.marker.get_dimensions();
			Rectangle<int16_t> abs_bounds = Rectangle<int16_t>(p, d);

			if (abs_bounds.contains(cursorpos))
			{
				path_img = path.second.path;
				show_path_img = path_img.is_valid();

				UI::get().show_map(Tooltip::Parent::WORLDMAP, path.second.title, path.second.description, path.second.map_ids[0], path.second.bolded);
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

		size_t i = Buttons::BT_LINK0;

		for (nl::node link : WorldMap["MapLink"])
		{
			nl::node l = link["link"];
			Texture link_image = l["linkImg"];

			link_images[i] = link_image;
			link_maps[i] = std::string(l["linkMap"]);

			buttons[i] = std::make_unique<AreaButton>(base_position - link_image.get_origin(), link_image.get_dimensions());
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