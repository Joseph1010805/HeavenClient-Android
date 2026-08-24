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

#include "../../Util/Multiplayer.h"
#include "../../Util/LocalServer.h"
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

		// HOST or JOIN, and the list of games found on the network.
		//
		// Not "online" and "offline" - there is no such distinction when
		// every device carries a server. There is only who is hosting, and
		// playing alone is hosting with nobody joining.
		//
		// It lives on the login screen because that is where the choice
		// matters, before anything connects, and top-left because the mount
		// clamps the bottom edge of the screen.
		enum class Mode : uint8_t
		{
			// Run the world here. Alone, or with others joining.
			HOST,
			// Play on somebody else's.
			JOIN
		};

		// The whole section, where the logo used to be. It has room to
		// EXPLAIN rather than just offer two buttons - what hosting means,
		// what is missing, who is out there - because the person reading it
		// may be setting a handheld up for the first time.
		//
		// On its own black plate: this is drawn over painted artwork, and
		// white text on a bright sky is unreadable.
		static constexpr int16_t SECTION_X = 498;
		static constexpr int16_t SECTION_Y = 10;
		static constexpr int16_t SECTION_W = 294;
		static constexpr int16_t SECTION_H = 300;

		static constexpr int16_t PAD = 10;
		static constexpr int16_t BUTTON_W = 132;
		static constexpr int16_t BUTTON_H = 34;

		// Where the explanation starts, under the buttons.
		static constexpr int16_t BODY_Y = SECTION_Y + BUTTON_H + 14;
		static constexpr int16_t LINE_H = 16;
		static constexpr int16_t ROW_H = 24;

		Rectangle<int16_t> mode_bounds(Mode which) const;
		Rectangle<int16_t> game_bounds(int16_t row) const;

		void choose_host();
		void choose_join();

		Mode mode = Mode::HOST;

		// What browsing has turned up, refreshed on a timer while the JOIN
		// list is showing. Names, never addresses - the address is nobody's
		// business and typing one is what this exists to abolish.
		std::vector<Multiplayer::Game> found;
		int16_t until_refresh = 1;

		// Wi-Fi Direct is the LAST resort, not the first.
		//
		// Searching the network the device is already on costs nothing and
		// covers every ordinary case - the house wifi, a phone's hotspot, a
		// travel router. Only when that turns up nothing is it worth making a
		// network of our own, which takes over the wifi radio and asks the
		// other person to accept a prompt.
		//
		// So: browse for this long first, and only then reach for it.
		static constexpr int16_t PATIENCE = 6 * 60;
		int16_t looking_for = 0;
		bool tried_wifi_direct = false;

		// What is stopping this device hosting, if anything. Refreshed when
		// HOST is chosen and every few seconds after, so finishing the setup
		// in Termux turns the list green without restarting the game.
		LocalServer::Readiness readiness;

		mutable Text check_line;
		mutable Text mode_label;
		mutable Text mode_hint;
		mutable Text game_name;

		Text version;
		Textfield account;
		Textfield password;
		Texture accountbg;
		Texture passwordbg;
		BoolPair<Texture> checkbox;

		bool saveid;
	};
}