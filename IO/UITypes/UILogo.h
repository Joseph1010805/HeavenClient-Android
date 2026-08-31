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

#include "../UI.h"

namespace ms
{
	class UILogo : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::START;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UILogo();

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

	private:
		// The still the sequence settles on, wherever it arrives there from.
		void draw_end() const;

		Animation Nexon;
		Animation Wizet;
		Texture WizetEnd;

		// The still the startup sequence settles on. Wizet's last frame by
		// default; the game's own wordmark when Map001.nx supplies one.
		Texture custom_logo;

		bool nexon_ended;
		bool wizet_ended;
		bool user_clicked;
	};
}