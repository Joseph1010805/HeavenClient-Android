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

#include "../../Graphics/Texture.h"
#include "../../Graphics/Text.h"

namespace ms
{
	// The wide boss health bar across the top of the screen.
	//
	// OpenStory has one of these and reads it from `MobGage.img`, which does
	// not exist in the artwork this client uses - the real path here is
	// UIWindow2.img/MobGage. Worth knowing before copying that file across.
	//
	// The gauge itself is eleven ONE PIXEL WIDE slices under Gage/1..11, each
	// with an empty (1x8) and a filled (1x10) variant. They are stretched to
	// length, and which of the eleven you use comes from the mob's own
	// hpTagColor / hpTagBgcolor - so Mushmom's bar is the colour Nexon chose
	// for Mushmom rather than a colour picked here.
	class MobGage
	{
	public:
		MobGage();

		void draw(Point<int16_t> position, float hp_percent) const;

		// `color` and `bgcolor` are the mob's hpTagColor / hpTagBgcolor.
		void set_mob(const std::string& name, int8_t color, int8_t bgcolor);

		bool is_active() const;
		void clear();

		int16_t width() const;

	private:
		// Clamped into 1..11, since a mob with no tag colour reads as 0 and
		// there is no Gage/0.
		static int8_t clamp_color(int8_t value);

		Texture bar_bg;
		Texture bar_fill;

		Text name_label;

		bool active = false;
		int8_t tag_color = 1;
		int8_t tag_bgcolor = 1;

		static constexpr int16_t BAR_W = 440;
		static constexpr int16_t BAR_H = 10;
	};
}
