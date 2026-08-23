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

#include "../Components/Textfield.h"
#include "../Template/BoolPair.h"

namespace ms
{
	class UILogin : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::LOGIN;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UILogin();

		void draw(float alpha) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursor_pos) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t id) override;

	private:
		void login();
		void open_url(uint16_t id);

		enum Buttons
		{
			BT_LOGIN,
			BT_REGISTER,
			BT_HOMEPAGE,
			BT_PASSLOST,
			BT_IDLOST,
			BT_SAVEID,
			BT_QUIT,
			NUM_BUTTONS
		};

		// Where this client is pointed, and a way to change it without
		// editing a file on the device.
		//
		// It sits on the login screen because that is where the choice
		// matters - before anything connects - and because the alternative,
		// finding a settings file in Android's storage on a handheld, is not
		// something anybody should have to do to play in the car.
		Rectangle<int16_t> server_switch_bounds() const;
		void toggle_server();

		mutable Text server_label;
		mutable Text server_hint;

		Text version;
		Textfield account;
		Textfield password;
		Texture accountbg;
		Texture passwordbg;
		BoolPair<Texture> checkbox;

		bool saveid;
	};
}