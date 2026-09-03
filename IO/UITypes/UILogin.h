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

		// WHAT THE POPUP IS ASKING, when there is one.
		//
		// THE HOST / JOIN PAIR IS GONE. It asked a question nobody at a table
		// thinks in: "are you a host or a joiner?" What people actually do is
		// look for the game somebody has already started, and start one if
		// nobody has. So the screen browses from the moment it opens and
		// shows what it finds, with CREATE A GAME as the last row - the thing
		// you press when the list has not got what you wanted in it.
		//
		// NOTHING IS STARTED UNTIL SOMETHING IS PRESSED, which was the real
		// point of the old pair and is kept. The old screen defaulted to HOST
		// and acted on it, so two handhelds both became Wi-Fi Direct group
		// owners within seconds of starting - and a group owner cannot join
		// another group. They sat there being networks at each other.
		// Browsing is passive; it cannot cause that.
		enum class Panel : uint8_t
		{
			// The login screen as it always was.
			NONE,
			// Six digits, to get into somebody else's game.
			CODE,
			// Six digits, chosen, to keep everybody else out of yours.
			CREATE,
			// The second step of hosting: HOW others reach this device. It is
			// its own screen rather than a tick box on the first, because a
			// box drawn in text does not read as something you can press -
			// and this is a real choice, not a setting to skim past.
			HOST_NETWORK
		};

		// THE FRAME'S INNER OPENING - the only rectangle this screen owns.
		//
		// Login.img/Common/frame is painted over the whole 800x600: a border
		// on all four sides and a taller bar along the bottom carrying the
		// Nexon and Wizet marks. Everything outside these bounds is UNDER
		// that border, and the border is opaque - so a control placed there
		// is not merely untidy, it is half invisible and the half that shows
		// runs off the edge of the screen.
		//
		// Measured off the running client rather than guessed: the border's
		// inner edge falls at x 31 / 769 and y 30 / 555.
		static constexpr int16_t FRAME_L = 31;
		static constexpr int16_t FRAME_T = 30;
		static constexpr int16_t FRAME_R = 769;
		static constexpr int16_t FRAME_B = 555;

		// The list of games, down the right. It grows with the list rather
		// than being drawn at its full depth from the start - four rows of
		// space with nothing in them reads as four games that failed to load,
		// which is the opposite of what an empty list means.
		static constexpr int16_t SECTION_W = 294;

		int16_t section_height() const;

		static constexpr int16_t SECTION_X = FRAME_R - 8 - SECTION_W;
		static constexpr int16_t SECTION_Y = FRAME_T + 8;

		static constexpr int16_t MAX_GAMES = 4;

		// The popup, over the middle of the screen where there is room to
		// explain, and now to hold a keypad.
		static constexpr int16_t POP_X = 210;
		static constexpr int16_t POP_Y = 58;
		static constexpr int16_t POP_W = 380;
		static constexpr int16_t POP_H = 424;

		static constexpr int16_t PAD = 10;
		static constexpr int16_t BUTTON_W = 132;
		static constexpr int16_t BUTTON_H = 34;
		static constexpr int16_t LINE_H = 16;
		static constexpr int16_t ROW_H = 30;

		static constexpr int16_t CODE_LEN = 6;

		// Rows in the list: 0..n-1 are found games, and the one after the
		// last game is always CREATE A GAME.
		Rectangle<int16_t> list_row(int16_t row) const;
		int16_t create_row() const;

		// The keypad, 3 across and 4 down: 1-9, then DELETE 0 CLEAR.
		Rectangle<int16_t> pad_key(int16_t which) const;

		// The two ways others can reach a host, as full-width rows.
		Rectangle<int16_t> choice_bounds(int16_t which) const;
		Rectangle<int16_t> commit_bounds() const;
		Rectangle<int16_t> cancel_bounds() const;

		void draw_panel_over(float alpha) const;
		void draw_keypad(int16_t top) const;
		bool keypad_pressed(Point<int16_t> at);

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

		// What has been tapped into the keypad so far, as digits. Never more
		// than CODE_LEN long.
		std::string typed;

		// Said only after a join is refused, and cleared the moment another
		// digit is pressed - an error that outlives the mistake is noise.
		bool code_refused = false;

		// Whether this device is actually hosting - which is only ever true
		// because somebody pressed CREATE and then chose a network.
		bool hosting = false;

		// One line under the list, when there is something to say that is not
		// about a particular game - "say yes to Termux", and the like. Empty
		// the rest of the time.
		std::string notice;

		// The code this device is hosting under, in the clear, so the screen
		// can keep showing it. The others have to be told it somehow, and
		// "somehow" is one person reading it off a screen out loud.
		std::string my_code;

		// WAITING FOR THE SERVER WE JUST STARTED.
		//
		// Pressing CREATE returns immediately - starting Cosmic takes
		// something like ten seconds, and for all of those the login button
		// was there, enabled, and would fail. Somebody presses it, gets
		// nothing, presses it again, and concludes the game is broken.
		//
		// So there is a wait screen, and it does NOT run on a timer. Ten
		// seconds is what it takes on a warm Thor; a cold one, or an RP5, or
		// a device that has just woken up, take as long as they take. It ASKS
		// - LocalServer::check() reports whether the server is ANSWERING, not
		// merely installed - and it stays up until the answer is yes.
		bool waiting_for_server = false;

		// Only so the screen can say something after a while. Nothing is
		// abandoned because of it.
		uint32_t waited = 0;

		mutable Text wait_title;
		mutable Text wait_line;

		void draw_server_wait(Point<int16_t> screen) const;

		// THE WAY OUT.
		//
		// A modal with no exit is worse than the problem it was put there to
		// solve. If the server never answers - it was never installed, the
		// setup was never run, Termux was killed - this screen would sit
		// there for ever with the game behind it and no way back. That is a
		// worse failure than the one it prevents, and it is the sort of thing
		// only a real device teaches you.
		Rectangle<int16_t> wait_dismiss_bounds() const;

		// WHICH GAME THE CODE IS BEING TYPED FOR - as a COPY, not an index.
		//
		// Discovery keeps running while the popup is open and rewrites the
		// list every half second. An index into a list that reorders itself
		// under you is a bug waiting for the moment a second handheld appears
		// mid-typing: the digits would be checked against one game's code and
		// then used to join a different one. Holding the game itself cannot
		// drift.
		bool picked = false;
		Multiplayer::Game target;

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

		// The code, in the one size that matters: big enough to read off a
		// screen from across a room, because that is literally how it gets
		// from the host to everybody else.
		mutable Text code_display;

		// "418703" -> "418 703". Six digits in a row is a number to
		// mistranscribe; two groups of three is a number people can say.
		static std::string spaced(const std::string& code);

		Text version;
		Textfield account;
		Textfield password;
		Texture accountbg;
		Texture passwordbg;
		BoolPair<Texture> checkbox;

		bool saveid;
	};
}