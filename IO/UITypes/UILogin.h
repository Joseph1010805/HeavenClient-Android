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

		// HOST and JOIN, and the popup behind each.
		//
		// NOTHING IS CHOSEN AT LAUNCH, and that is the important part rather
		// than a matter of taste. The old screen defaulted to HOST and acted
		// on it immediately - starting the server and, with no wifi, making
		// itself a Wi-Fi Direct group. So both handhelds became group owners
		// within seconds of starting, and a group owner cannot join another
		// group: they sat there being networks at each other. Doing nothing
		// until asked is what fixes that at the root.
		//
		// Each button opens a popup that shows what is going to happen and
		// whether it can, and carries a second button to commit. Backing out
		// undoes it, so a device that hosted can go and join instead.
		enum class Panel : uint8_t
		{
			// The login screen as it always was.
			NONE,
			HOST,
			// The second step of hosting: HOW others reach this device. It is
			// its own screen rather than a tick box on the first, because a
			// box drawn in text does not read as something you can press -
			// and this is a real choice, not a setting to skim past.
			HOST_NETWORK,
			JOIN
		};

		// The section on the right, where the logo used to be.
		static constexpr int16_t SECTION_X = 498;
		static constexpr int16_t SECTION_Y = 10;
		static constexpr int16_t SECTION_W = 294;
		static constexpr int16_t SECTION_H = 116;

		// The popup, over the middle of the screen where there is room to
		// explain and to list.
		static constexpr int16_t POP_X = 210;
		static constexpr int16_t POP_Y = 70;
		static constexpr int16_t POP_W = 380;
		static constexpr int16_t POP_H = 300;

		static constexpr int16_t PAD = 10;
		static constexpr int16_t BUTTON_W = 132;
		static constexpr int16_t BUTTON_H = 34;
		static constexpr int16_t LINE_H = 16;
		static constexpr int16_t ROW_H = 24;

		Rectangle<int16_t> section_button(Panel which) const;
		// The two ways others can reach a host, as full-width rows.
		Rectangle<int16_t> choice_bounds(int16_t which) const;
		Rectangle<int16_t> commit_bounds() const;
		Rectangle<int16_t> cancel_bounds() const;
		Rectangle<int16_t> game_bounds(int16_t row) const;

		void draw_panel_over(float alpha) const;
		void open_panel(Panel which);
		void close_panel();
		//  picks Wi-Fi Direct over whatever wifi is already
		// there. Being ON a network is not the same as being able to reach
		// the handheld beside you - guest and hotel wifi routinely block
		// devices from talking to each other - so this is a choice and not
		// only an automatic fallback.
		void commit_host(bool own_network);
		void commit_join();
		void stop_hosting();

		Panel panel = Panel::NONE;

		// Whether this device is actually hosting - which is only ever true
		// because somebody pressed the second HOST button.
		bool hosting = false;

		// Which game in the list is picked. Nothing until one is, and JOIN
		// will not commit without it.
		int16_t picked = -1;

		std::vector<Multiplayer::Game> found;
		int16_t until_refresh = 1;

		// Wi-Fi Direct is the LAST resort, not the first. Searching a network
		// the device is already on costs nothing and covers the house wifi, a
		// phone's hotspot, a travel router. Making a network of our own takes
		// over the radio and puts a prompt in front of the other person.
		static constexpr int16_t PATIENCE = 6 * 60;
		int16_t looking_for = 0;
		bool tried_wifi_direct = false;

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