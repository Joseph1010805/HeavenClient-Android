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
#include "MobGage.h"

#include <nlnx/nx.hpp>

namespace ms
{
	MobGage::MobGage()
	{
		name_label = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE);
	}

	int8_t MobGage::clamp_color(int8_t value)
	{
		if (value < 1)
			return 1;

		if (value > 11)
			return 11;

		return value;
	}

	void MobGage::set_mob(const std::string& name, int8_t color, int8_t bgcolor)
	{
		tag_color = clamp_color(color);
		tag_bgcolor = clamp_color(bgcolor);

		nl::node gage = nl::nx::ui["UIWindow2.img"]["MobGage"]["Gage"];

		// 0 is the empty slice, 1 the filled one.
		bar_bg = gage[std::to_string(tag_bgcolor)]["0"];
		bar_fill = gage[std::to_string(tag_color)]["1"];

		name_label.change_text(name);
		active = true;
	}

	void MobGage::draw(Point<int16_t> position, float percent) const
	{
		if (!active)
			return;

		if (percent < 0.0f)
			percent = 0.0f;
		else if (percent > 1.0f)
			percent = 1.0f;

		// Both slices are a single pixel wide, so the second DrawArgument
		// argument is a stretch-to rather than an offset.
		bar_bg.draw(DrawArgument(position, Point<int16_t>(BAR_W, BAR_H)));

		int16_t filled = static_cast<int16_t>(BAR_W * percent);

		if (filled > 0)
			bar_fill.draw(DrawArgument(position, Point<int16_t>(filled, BAR_H)));

		name_label.draw(position + Point<int16_t>(BAR_W / 2, -18));
	}

	bool MobGage::is_active() const
	{
		return active;
	}

	void MobGage::clear()
	{
		active = false;
	}

	int16_t MobGage::width() const
	{
		return BAR_W;
	}
}
