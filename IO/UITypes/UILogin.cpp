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
#include "UILogin.h"

#include "../../Graphics/GraphicsGL.h"
#include "../../Util/LocalServer.h"
#include "../../Util/Silent.h"
#include "../../Util/Carry.h"
#include "../../Net/Session.h"
#include "UILoginwait.h"
#include "UILoginNotice.h"
#include "UINotice.h"

#include "../UI.h"
#include "../SecondScreen.h"

#include "../../Constants.h"

#include "../Components/MapleButton.h"
#include "../Audio/Audio.h"

#include "../Net/Packets/LoginPackets.h"

#ifdef WIN32
#include <windows.h>
#endif

#include <nlnx/nx.hpp>

namespace ms
{
	namespace
	{
		// The login panel and the sign both used to sit in the middle of the
		// screen, stacked on top of each other, which was fine over a static
		// backdrop and wastes a moving one. They are pushed into opposite top
		// corners so the artwork between them stays visible.
		//
		// Every coordinate in this file was authored against the panel's old
		// centred position, so rather than renumber each one they are all
		// shifted by this. The panel art (signboard) is 243x132 with its origin
		// at 115,66, so drawn at 391,330 its top-left corner was 276,264 -
		// which is what this offset measures from.
		constexpr Point<int16_t> PANEL = Point<int16_t>(40 - 276, 45 - 264);

		// Top right, inside the frame's inner edge (about x 769). The sign is
		// 240 wide, and this clears the version text at y 1.
		constexpr Point<int16_t> LOGO_POS = Point<int16_t>(530, 16);

		// THE KEYPAD, not a text field.
		//
		// The code is six digits and nothing else, and a numeric pad drawn in
		// the popup can be pressed where the popup is - on the top screen -
		// without depending on the panel keyboard being up, being reached, or
		// existing at all on a device with one display.
		const char* PAD_LABELS[12] =
		{
			"1", "2", "3",
			"4", "5", "6",
			"7", "8", "9",
			"DEL", "0", "CLR"
		};

		// Measured down from the popup: title, hint, the six boxes, then this.
		constexpr int16_t PAD_TOP = 168;
	}

	std::string UILogin::spaced(const std::string& code)
	{
		if (code.size() != 6)
			return code;

		return code.substr(0, 3) + "  " + code.substr(3);
	}

	UILogin::UILogin() : UIElement(Point<int16_t>(0, 0), Point<int16_t>(Constants::Constants::get().get_viewwidth(), Constants::Constants::get().get_viewheight()))
	{
		Music("BgmUI.img/Title").play();

		std::string version_text = Configuration::get().get_version();
		mode_label = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE);
		mode_hint = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::LIGHTGREY);
		game_name = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);
		check_line = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);
		code_display = Text(Text::Font::A18M, Text::Alignment::CENTER, Color::Name::WHITE);

		// LOOK FOR GAMES FROM THE MOMENT THE SCREEN OPENS.
		//
		// Browsing is passive - it announces nothing, starts nothing and
		// takes no radio - so there is no reason to make somebody press a
		// button before the search that they were always going to want.
		// Announcing is the thing that has to be asked for, and still is.
		Multiplayer::start_browsing();

		// NOTHING happens at launch. Not hosting, not announcing, not making
		// a network. See UILogin.h for why that matters rather than being a
		// matter of taste.

		version = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::LEMONGRASS, "Ver. " + version_text);

		// Custom artwork, built by tools/make_assets.py into Map001.nx. The
		// background is a video: an ordinary NX animation of numbered frames,
		// so nothing here has to know it came from an mp4.
		//
		// The frames are authored well under 800x600 and stretched to fill the
		// screen. Every frame stays in the texture atlas for as long as this
		// screen is up, so authoring them full size would crowd out the rest
		// of the UI - and the art is a soft painted background that loses very
		// little to being scaled up.
		// Falls back to the stock artwork when that file is absent, so a
		// checkout without it still looks like MapleStory rather than showing
		// a blank screen.
		nl::node custom = nl::nx::map001["Custom"];

		nl::node title = nl::nx::ui["Login.img"]["Title"];
		nl::node common = nl::nx::ui["Login.img"]["Common"];

		if (custom["LoginBg"])
		{
			// INSIDE THE FRAME, NOT UNDER IT.
			//
			// Stretched corner to corner was right for a video - it had no
			// edges of its own, and the frame's border finished it. This is a
			// still with a painted border already in it, so full-bleed put two
			// borders on the screen and hid the outer 30px of the good one.
			//
			// It fills the frame's opening exactly now. Nothing of it is drawn
			// where it cannot be seen.
			sprites.emplace_back(custom["LoginBg"], DrawArgument(
				Point<int16_t>(FRAME_L, FRAME_T),
				Point<int16_t>(FRAME_R - FRAME_L, FRAME_B - FRAME_T)));

			// THE WORDMARK, BOTTOM LEFT.
			//
			// It went missing when the login video did - the video had the
			// name painted into it, so nothing ever drew the logo here. Down
			// in the corner rather than across the middle: the HOST / JOIN
			// panel is what somebody setting up a handheld needs to see
			// first, and a name is not an instruction.
			if (custom["Logo"])
			{
				Texture mark = custom["Logo"];
				Point<int16_t> size = mark.get_dimensions();

				// Half size. It is drawn for a splash screen and at full size
				// it is a third of the width of the form beside it.
				Point<int16_t> to(
					static_cast<int16_t>(size.x() / 2),
					static_cast<int16_t>(size.y() / 2));

				// Measured from the frame's bottom edge, not the screen's -
				// off the screen's it sat 13px into the Nexon bar, which is
				// opaque, so the bottom of the name was simply gone.
				Point<int16_t> at(
					static_cast<int16_t>(FRAME_L + 12),
					static_cast<int16_t>(FRAME_B - 12 - to.y()));

				sprites.emplace_back(custom["Logo"], DrawArgument(at, to));
			}
		}
		else
		{
			nl::node map = nl::nx::map001["Back"]["login.img"];
			nl::node back = map["back"];
			nl::node ani = map["ani"];

			sprites.emplace_back(back["11"], Point<int16_t>(400, 300));
			sprites.emplace_back(ani["17"], Point<int16_t>(129, 283));
			sprites.emplace_back(ani["18"], Point<int16_t>(306, 252));
			sprites.emplace_back(ani["19"], Point<int16_t>(379, 207));
			sprites.emplace_back(back["35"], Point<int16_t>(399, 260));
		}

		sprites.emplace_back(title["signboard"], Point<int16_t>(391, 330) + PANEL);
		sprites.emplace_back(common["frame"], Point<int16_t>(400, 300));

		buttons[Buttons::BT_LOGIN] = std::make_unique<MapleButton>(title["BtLogin"], Point<int16_t>(454, 279) + PANEL);
		buttons[Buttons::BT_SAVEID] = std::make_unique<MapleButton>(title["BtLoginIDSave"], Point<int16_t>(303, 332) + PANEL);
		buttons[Buttons::BT_IDLOST] = std::make_unique<MapleButton>(title["BtLoginIDLost"], Point<int16_t>(375, 332) + PANEL);
		buttons[Buttons::BT_PASSLOST] = std::make_unique<MapleButton>(title["BtPasswdLost"], Point<int16_t>(447, 332) + PANEL);
		buttons[Buttons::BT_REGISTER] = std::make_unique<MapleButton>(title["BtNew"], Point<int16_t>(291, 352) + PANEL);
		buttons[Buttons::BT_HOMEPAGE] = std::make_unique<MapleButton>(title["BtHomePage"], Point<int16_t>(363, 352) + PANEL);
		buttons[Buttons::BT_QUIT] = std::make_unique<MapleButton>(title["BtQuit"], Point<int16_t>(435, 352) + PANEL);

		checkbox[false] = title["check"]["0"];
		checkbox[true] = title["check"]["1"];

		account = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, Rectangle<int16_t>(Point<int16_t>(296, 279) + PANEL, Point<int16_t>(446, 303) + PANEL), 12);

		account.set_key_callback
		(
			KeyAction::Id::TAB, [&]
			{
				account.set_state(Textfield::State::NORMAL);
				password.set_state(Textfield::State::FOCUSED);
			}
		);

		account.set_enter_callback
		(
			[&](std::string msg)
			{
				login();
			}
		);

		accountbg = title["ID"];

		password = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, Rectangle<int16_t>(Point<int16_t>(296, 305) + PANEL, Point<int16_t>(446, 329) + PANEL), 12);

		password.set_key_callback
		(
			KeyAction::Id::TAB, [&]
			{
				account.set_state(Textfield::State::FOCUSED);
				password.set_state(Textfield::State::NORMAL);
			}
		);

		password.set_enter_callback
		(
			[&](std::string msg)
			{
				login();
			}
		);

		password.set_cryptchar('*');
		passwordbg = title["PW"];

		saveid = Setting<SaveLogin>::get().load();

		if (saveid)
		{
			account.change_text(Setting<DefaultAccount>::get().load());
			password.set_state(Textfield::State::FOCUSED);
			password.change_text(Setting<DefaultPassword>::get().load());
		}
		else
		{
			account.set_state(Textfield::State::FOCUSED);
		}

		if (Configuration::get().get_auto_login())
		{
			UI::get().emplace<UILoginwait>([]() {});

			auto loginwait = UI::get().get_element<UILoginwait>();

			if (loginwait && loginwait->is_active())
				LoginPacket(
					Configuration::get().get_auto_acc(),
					Configuration::get().get_auto_pass()
				).dispatch();
		}
	}

	void UILogin::draw(float alpha) const
	{
		UIElement::draw(alpha);

		// Bottom right of the OPENING. At y 1 it was above the frame's top
		// edge entirely - drawn every frame, visible in none of them.
		version.draw(position + Point<int16_t>(FRAME_R - 70, FRAME_B - 22));

		// THE LIST OF GAMES, where the logo used to be.
		GraphicsGL::get().drawrectangle(
			position.x() + SECTION_X, position.y() + SECTION_Y,
			SECTION_W, section_height(), 0.0f, 0.0f, 0.0f, 0.82f);

		int16_t left = position.x() + SECTION_X + PAD;
		int16_t mid = position.x() + SECTION_X + SECTION_W / 2;

		mode_label.change_text(hosting ? "YOUR GAME IS OPEN" : "GAMES NEARBY");
		mode_label.draw(Point<int16_t>(mid, position.y() + SECTION_Y + PAD));

		if (hosting)
		{
			// THE CODE, LARGE, AND IT STAYS THERE.
			//
			// The host has to read it out to everybody else, possibly more
			// than once and possibly an hour later when somebody's handheld
			// runs flat. A number shown once at creation and then thrown away
			// would have them restarting the game to see it again.
			code_display.change_text(spaced(my_code));
			code_display.draw(Point<int16_t>(mid, position.y() + SECTION_Y + 46));

			mode_hint.change_text("Tell the others this code.");
			mode_hint.draw(Point<int16_t>(left, position.y() + SECTION_Y + 92));

			mode_hint.change_text("They pick your name and type it in.");
			mode_hint.draw(Point<int16_t>(left, position.y() + SECTION_Y + 92 + LINE_H));

			Rectangle<int16_t> stop = list_row(0);

			GraphicsGL::get().drawrectangle(
				stop.left(), stop.top() + 78, stop.width(), stop.height(),
				0.30f, 0.13f, 0.13f, 1.0f);

			mode_label.change_text("CLOSE THE GAME");
			mode_label.draw(Point<int16_t>(mid, stop.top() + 78 + 7));

			if (panel != Panel::NONE)
				draw_panel_over(alpha);

			account.draw(position);
			password.draw(position);

			if (account.get_state() == Textfield::State::NORMAL && account.empty())
				accountbg.draw(DrawArgument(position + Point<int16_t>(291, 279) + PANEL));

			if (password.get_state() == Textfield::State::NORMAL && password.empty())
				passwordbg.draw(DrawArgument(position + Point<int16_t>(291, 305) + PANEL));

			checkbox[saveid].draw(DrawArgument(position + Point<int16_t>(291, 335) + PANEL));

			if (waiting_for_server)
				draw_server_wait(Point<int16_t>(800, 600));

			return;
		}

		int16_t shown = static_cast<int16_t>(
			found.size() < MAX_GAMES ? found.size() : MAX_GAMES);

		for (int16_t i = 0; i < shown; i++)
		{
			Rectangle<int16_t> at = list_row(i);

			GraphicsGL::get().drawrectangle(
				at.left(), at.top(), at.width(), at.height(),
				0.11f, 0.13f, 0.16f, 1.0f);

			game_name.change_text(found[i].name);
			game_name.draw(Point<int16_t>(at.left() + 10, at.top() + 5));

			// A game with no code is one an older build is hosting. Say so
			// rather than silently letting anybody walk in, which is the sort
			// of difference that only shows up when it matters.
			mode_hint.change_text(found[i].code ? "needs a code" : "open");
			mode_hint.draw(Point<int16_t>(at.right() - 84, at.top() + 6));
		}

		// CREATE IS ALWAYS THE LAST ROW.
		//
		// Not a separate button somewhere else on the screen: it is the
		// answer to "my game is not in this list", so it belongs at the
		// bottom of the list where that thought happens.
		Rectangle<int16_t> make = list_row(create_row());

		GraphicsGL::get().drawrectangle(
			make.left(), make.top(), make.width(), make.height(),
			0.16f, 0.30f, 0.20f, 1.0f);

		mode_label.change_text("+  CREATE A GAME");
		mode_label.draw(Point<int16_t>(mid, make.top() + 7));

		// Always say it is still looking. Discovery is slower than a person,
		// and a list that looks settled a second before the other handheld
		// appears reads as a failure.
		int16_t after = make.bottom() + 8;

		if (!found.empty())
			mode_hint.change_text("Still looking...");
		else if (!Multiplayer::wifi_radio_on())
			mode_hint.change_text("Turn WIFI ON - it needs the radio.");
		else if (tried_wifi_direct)
			mode_hint.change_text("No network - looking for a device...");
		else
			mode_hint.change_text("Looking for games...");

		mode_hint.draw(Point<int16_t>(left, after));

		if (!notice.empty())
		{
			mode_hint.change_text(notice);
			mode_hint.draw(Point<int16_t>(left, after + LINE_H));
		}
		else if (found.empty())
		{
			mode_hint.change_text("Or create one and let them join you.");
			mode_hint.draw(Point<int16_t>(left, after + LINE_H));
		}

		if (panel != Panel::NONE)
			draw_panel_over(alpha);

		account.draw(position);
		password.draw(position);

		if (account.get_state() == Textfield::State::NORMAL && account.empty())
			accountbg.draw(DrawArgument(position + Point<int16_t>(291, 279) + PANEL));

		if (password.get_state() == Textfield::State::NORMAL && password.empty())
			passwordbg.draw(DrawArgument(position + Point<int16_t>(291, 305) + PANEL));

		checkbox[saveid].draw(DrawArgument(position + Point<int16_t>(291, 335) + PANEL));

		if (waiting_for_server)
			draw_server_wait(Point<int16_t>(800, 600));
	}

	int16_t UILogin::section_height() const
	{
		// Hosting shows the code, two lines about it, and the way to stop -
		// a fixed shape, because none of it comes and goes.
		// 30 for the title, 78 down to the CLOSE row - the gap the code and
		// its two lines sit in - then the row itself and a margin.
		if (hosting)
			return static_cast<int16_t>(30 + 78 + (ROW_H - 4) + PAD);

		// Otherwise: the title, a row per game, the CREATE row, and one or
		// two lines of what the search is doing.
		int16_t rows = static_cast<int16_t>(create_row() + 1);
		int16_t said = static_cast<int16_t>(found.empty() ? LINE_H * 2 : LINE_H);

		return static_cast<int16_t>(30 + rows * ROW_H + 8 + said + PAD);
	}

	int16_t UILogin::create_row() const
	{
		int16_t shown = static_cast<int16_t>(
			found.size() < MAX_GAMES ? found.size() : MAX_GAMES);

		return shown;
	}

	Rectangle<int16_t> UILogin::list_row(int16_t row) const
	{
		Point<int16_t> at = position + Point<int16_t>(
			SECTION_X + PAD,
			static_cast<int16_t>(SECTION_Y + 30 + row * ROW_H));

		return Rectangle<int16_t>(at, at + Point<int16_t>(
			static_cast<int16_t>(SECTION_W - PAD * 2), ROW_H - 4));
	}

	Rectangle<int16_t> UILogin::pad_key(int16_t which) const
	{
		constexpr int16_t KEY_W = 114;
		constexpr int16_t KEY_H = 42;
		constexpr int16_t GAP = 8;

		int16_t col = which % 3;
		int16_t row = which / 3;

		Point<int16_t> at = position + Point<int16_t>(
			static_cast<int16_t>(POP_X + PAD + col * (KEY_W + GAP)),
			static_cast<int16_t>(PAD_TOP + row * (KEY_H + GAP)));

		return Rectangle<int16_t>(at, at + Point<int16_t>(KEY_W, KEY_H));
	}

	Rectangle<int16_t> UILogin::choice_bounds(int16_t which) const
	{
		// Full width and tall enough to be obviously pressable. The first
		// attempt at this was a tick box drawn in text, which nobody could
		// tell was a control at all.
		Point<int16_t> at = position + Point<int16_t>(
			POP_X + PAD, POP_Y + 62 + which * 84);

		return Rectangle<int16_t>(at, at + Point<int16_t>(POP_W - 2 * PAD, 38));
	}

	Rectangle<int16_t> UILogin::commit_bounds() const
	{
		Point<int16_t> at = position + Point<int16_t>(
			POP_X + POP_W - PAD - 120, POP_Y + POP_H - PAD - BUTTON_H);

		return Rectangle<int16_t>(at, at + Point<int16_t>(120, BUTTON_H));
	}

	Rectangle<int16_t> UILogin::cancel_bounds() const
	{
		Point<int16_t> at = position + Point<int16_t>(
			POP_X + PAD, POP_Y + POP_H - PAD - BUTTON_H);

		return Rectangle<int16_t>(at, at + Point<int16_t>(110, BUTTON_H));
	}

	void UILogin::draw_keypad(int16_t top) const
	{
		(void)top;

		for (int16_t i = 0; i < 12; i++)
		{
			Rectangle<int16_t> at = pad_key(i);
			bool plain = i < 9 || i == 10;

			GraphicsGL::get().drawrectangle(
				at.left(), at.top(), at.width(), at.height(),
				plain ? 0.16f : 0.13f,
				plain ? 0.18f : 0.13f,
				plain ? 0.22f : 0.16f, 1.0f);

			mode_label.change_text(PAD_LABELS[i]);
			mode_label.draw(Point<int16_t>(
				at.left() + at.width() / 2, at.top() + (at.height() - 14) / 2));
		}
	}

	bool UILogin::keypad_pressed(Point<int16_t> at)
	{
		for (int16_t i = 0; i < 12; i++)
		{
			if (!pad_key(i).contains(at))
				continue;

			code_refused = false;

			if (i == 9)
			{
				if (!typed.empty())
					typed.pop_back();
			}
			else if (i == 11)
			{
				typed.clear();
			}
			else if (static_cast<int16_t>(typed.size()) < CODE_LEN)
			{
				typed.push_back(i == 10 ? '0' : static_cast<char>('1' + i));
			}

			return true;
		}

		return false;
	}

	void UILogin::draw_panel_over(float alpha) const
	{
		// Dim everything behind it, so the popup is plainly the thing being
		// answered rather than more decoration on an already busy screen.
		GraphicsGL::get().drawrectangle(
			position.x(), position.y(),
			Constants::Constants::get().get_viewwidth(),
			Constants::Constants::get().get_viewheight(), 0.0f, 0.0f, 0.0f, 0.55f);

		GraphicsGL::get().drawrectangle(
			position.x() + POP_X, position.y() + POP_Y, POP_W, POP_H,
			0.04f, 0.05f, 0.07f, 0.97f);

		int16_t left = position.x() + POP_X + PAD;
		int16_t mid = position.x() + POP_X + POP_W / 2;
		int16_t y = position.y() + POP_Y + PAD;

		bool can_commit = false;

		if (panel == Panel::HOST_NETWORK)
		{
			mode_label.change_text("HOW DO THEY REACH YOU?");
			mode_label.draw(Point<int16_t>(mid, y));
			y += 24;

			mode_hint.change_text("Everyone plays in YOUR world.");
			mode_hint.draw(Point<int16_t>(left, y));

			bool on_wifi = Multiplayer::on_network();
			bool can_make = Multiplayer::wifi_direct_supported()
				&& Multiplayer::wifi_radio_on();

			struct Choice { bool ok; const char* title; const char* line1; const char* line2; };

			Choice choices[2] =
			{
				{ on_wifi, "USE THIS WIFI",
					on_wifi ? "Everyone joins over the wifi you are on."
						: "Not available - this device is on no wifi.",
					on_wifi ? "Best when it works. Try this first."
						: "" },
				{ can_make, "MAKE MY OWN NETWORK",
					can_make ? "This device becomes the network itself."
						: "Not available - turn the WIFI RADIO on.",
					can_make ? "For a car, or wifi that blocks devices."
						: "It needs the radio, not a network." }
			};

			for (int16_t i = 0; i < 2; i++)
			{
				Rectangle<int16_t> at = choice_bounds(i);
				const Choice& choice = choices[i];

				GraphicsGL::get().drawrectangle(
					at.left(), at.top(), at.width(), at.height(),
					choice.ok ? 0.18f : 0.10f,
					choice.ok ? 0.34f : 0.11f,
					choice.ok ? 0.22f : 0.13f, 1.0f);

				mode_label.change_text(choice.title);
				mode_label.draw(Point<int16_t>(
					at.left() + at.width() / 2, at.top() + 8));

				check_line.change_text(choice.line1);
				check_line.draw(Point<int16_t>(at.left(), at.bottom() + 3));

				check_line.change_text(choice.line2);
				check_line.draw(Point<int16_t>(at.left(), at.bottom() + 3 + LINE_H));
			}

			// The choice screen has no single commit - each row IS one - so
			// only BACK is drawn under it.
			Rectangle<int16_t> only_back = cancel_bounds();

			GraphicsGL::get().drawrectangle(
				only_back.left(), only_back.top(),
				only_back.width(), only_back.height(),
				0.14f, 0.14f, 0.17f, 1.0f);

			mode_label.change_text("BACK");
			mode_label.draw(Point<int16_t>(
				only_back.left() + only_back.width() / 2,
				only_back.top() + (only_back.height() - 14) / 2));

			return;
		}

		bool joining = panel == Panel::CODE;

		mode_label.change_text(joining ? "ENTER THE CODE" : "CHOOSE A CODE");
		mode_label.draw(Point<int16_t>(mid, y));
		y += 24;

		if (joining && picked)
			mode_hint.change_text("Ask whoever is hosting " + target.name + ".");
		else if (joining)
			mode_hint.change_text("Ask whoever is hosting.");
		else
			mode_hint.change_text("Six digits. You will read it out to them.");

		mode_hint.draw(Point<int16_t>(left, y));

		// THE SIX BOXES.
		//
		// Six separate boxes rather than one field, because the length is the
		// instruction: nobody has to be told how many digits when they can
		// see four empty squares left.
		constexpr int16_t BOX_W = 54;
		constexpr int16_t BOX_H = 40;
		constexpr int16_t BOX_GAP = 6;

		int16_t used = CODE_LEN * BOX_W + (CODE_LEN - 1) * BOX_GAP;
		int16_t box_x = mid - used / 2;
		int16_t box_y = position.y() + POP_Y + 56;

		for (int16_t i = 0; i < CODE_LEN; i++)
		{
			int16_t at = box_x + i * (BOX_W + BOX_GAP);
			bool filled = i < static_cast<int16_t>(typed.size());

			GraphicsGL::get().drawrectangle(
				at, box_y, BOX_W, BOX_H,
				code_refused ? 0.34f : (filled ? 0.18f : 0.11f),
				code_refused ? 0.12f : (filled ? 0.21f : 0.12f),
				code_refused ? 0.12f : (filled ? 0.26f : 0.15f), 1.0f);

			if (filled)
			{
				code_display.change_text(std::string(1, typed[i]));
				code_display.draw(Point<int16_t>(at + BOX_W / 2, box_y + 4));
			}
		}

		draw_keypad(PAD_TOP);

		int16_t said = position.y() + PAD_TOP + 4 * 50 + 4;

		if (code_refused)
		{
			check_line.change_text("That code does not match. Try again.");
			check_line.draw(Point<int16_t>(left, said));
		}
		else if (!joining)
		{
			// Only what is WRONG, and only while creating. A green tick list
			// is reassurance for whoever built the thing; a person starting a
			// game wants to be told when something is in the way and left
			// alone when nothing is.
			//
			// The two are not the same kind of thing and must not be drawn as
			// if they were. Termux missing is a WALL - there is no server to
			// start and nothing the button can do about it. Permission not
			// granted yet is a STEP: pressing on is what makes Termux ask.
			int16_t at = said;

			if (!readiness.termux)
			{
				check_line.change_text("[X] Termux is not installed - see docs_OFFLINE.md");
				check_line.draw(Point<int16_t>(left, at));
				at += LINE_H;
			}
			else if (!readiness.permission)
			{
				check_line.change_text("Termux will ask to allow this. Say yes,");
				check_line.draw(Point<int16_t>(left, at));
				at += LINE_H;

				check_line.change_text("then press CREATE again.");
				check_line.draw(Point<int16_t>(left, at));
				at += LINE_H;
			}
		}

		bool full = static_cast<int16_t>(typed.size()) == CODE_LEN;

		can_commit = joining
			? (full && picked)
			: (full && readiness.can_try());

		Rectangle<int16_t> go = commit_bounds();

		GraphicsGL::get().drawrectangle(
			go.left(), go.top(), go.width(), go.height(),
			can_commit ? 0.18f : 0.10f,
			can_commit ? 0.34f : 0.11f,
			can_commit ? 0.22f : 0.13f, 1.0f);

		mode_label.change_text(joining ? "JOIN" : "NEXT");
		mode_label.draw(Point<int16_t>(
			go.left() + go.width() / 2, go.top() + (go.height() - 14) / 2));

		Rectangle<int16_t> back = cancel_bounds();

		GraphicsGL::get().drawrectangle(
			back.left(), back.top(), back.width(), back.height(),
			0.14f, 0.14f, 0.17f, 1.0f);

		mode_label.change_text("BACK");
		mode_label.draw(Point<int16_t>(
			back.left() + back.width() / 2, back.top() + (back.height() - 14) / 2));
	}

	void UILogin::open_panel(Panel which)
	{
		panel = which;
		code_refused = false;

		if (which == Panel::HOST_NETWORK)
			return;

		typed.clear();

		// The list stops moving while a code is being typed. Nothing here
		// reads it any more - target holds the game - but a list shuffling
		// itself behind a popup is a thing people notice and mistrust.
		Multiplayer::stop_browsing();

		if (which == Panel::CREATE)
		{
			// Only LOOK. Nothing is started until a network is chosen.
			readiness = LocalServer::check();

			// Creating a game means not looking for one. Browsing while about
			// to host is where two handhelds used to deadlock, each being a
			// network at the other.
			Multiplayer::stop_browsing();
		}
	}

	void UILogin::close_panel()
	{
		panel = Panel::NONE;
		typed.clear();
		code_refused = false;
		picked = false;

		// Back to watching for games, unless this device is now one.
		if (!hosting)
			Multiplayer::start_browsing();
	}

	void UILogin::stop_hosting()
	{
		Multiplayer::stop_hosting();
		Multiplayer::remove_group();

		hosting = false;
		my_code.clear();

		Multiplayer::start_browsing();
	}

	void UILogin::commit_host(bool own_network)
	{
		if (!readiness.can_try())
		{
			Silent::report("UILogin::commit_host",
				std::string("cannot host - termux=") + (readiness.termux ? "1" : "0")
				+ " permission=" + (readiness.permission ? "1" : "0"));

			return;
		}

		// NO PERMISSION YET IS NOT A REFUSAL. Going on is what makes Termux
		// ask for it - see LocalServer::Readiness::can_try. The attempt below
		// will not start anything this time; it puts the dialog on screen,
		// and the second press is the one that works.
		if (!readiness.permission)
		{
			LocalServer::start();

			notice = "Say yes to Termux, then press CREATE again.";

			panel = Panel::NONE;
			typed.clear();

			return;
		}

		// Point at ourselves and start the server.
		LocalServer::set_offline(true);
		LocalServer::start();

		// HOME. Hosting our own world means these characters live here again,
		// so the card is ours to keep current - see Carry::update.
		Carry::get().set_visiting(false, "");

		// And do not let anybody try to log in until it answers.
		waiting_for_server = true;
		waited = 0;

		// Then become the network, if that is what was chosen.
		if (own_network
			&& Multiplayer::wifi_direct_supported()
			&& Multiplayer::wifi_radio_on())
			Multiplayer::create_group();

		my_code = typed;

		Multiplayer::stop_browsing();
		Multiplayer::start_hosting(
			Multiplayer::suggested_name(), Multiplayer::code_hash(my_code));

		hosting = true;

		panel = Panel::NONE;
		typed.clear();
		picked = false;
	}

	void UILogin::commit_join()
	{
		if (!picked)
			return;

		// A host announcing no code at all is an older build; there is
		// nothing to check against, so the digits are simply ignored rather
		// than refused, which would be refusing an empty test.
		if (target.code && Multiplayer::code_hash(typed) != target.code)
		{
			code_refused = true;

			return;
		}

		Setting<ServerIP>::get().save(target.address);
		Configuration::get().save();

		// SEND THE CHARACTERS AHEAD.
		//
		// This is the moment a device commits to somebody else's world, and
		// it has to happen BEFORE logging in: the account does not exist over
		// there yet, so the character list would come back empty and the
		// person would conclude their characters were gone.
		//
		// Failure is not fatal and must not block the join. A host on an
		// older build has no carry port at all, and joining it to play a
		// fresh character is a perfectly reasonable thing to want. Silent has
		// the reason either way.
		Carry::get().set_visiting(true, target.address);

		if (Carry::get().has_card())
		{
			if (!Carry::get().deliver(target.address))
			{
				Silent::report("UILogin",
					"joined " + target.address + " without the card: "
					+ Carry::get().trouble());
			}
		}

		close_panel();
	}

	void UILogin::update()
	{
		// THE SERVER WAIT IS CHECKED FIRST, BEFORE ANY EARLY RETURN.
		//
		// This used to live at the bottom, and the very first thing below is
		// an early return that is ALWAYS taken once hosting has been
		// committed - so the one piece of code that could close the wait
		// screen sat behind a return that was always taken. The screen was
		// not stuck for want of an answer; nothing was asking the question.
		//
		// LocalServer::check knocks on the login port at most every two
		// seconds, on its own thread, so calling it every frame is cheap.
		if (waiting_for_server)
		{
			waited++;

			readiness = LocalServer::check();

			if (readiness.server)
				waiting_for_server = false;
		}

		UIElement::update();

		account.update(position);
		password.update(position);

		// SOMETHING MUST HAVE FOCUS, OR THERE IS NOWHERE TO TYPE -
		// BUT ONLY WHERE THE KEYBOARD IS ON THE OTHER SCREEN.
		//
		// On a two-screen handheld the box is behind glass at the top of the
		// device and the keys are on the panel below, so nothing can ever be
		// tapped to give it focus: a key press arrived with no destination
		// and did nothing, which is most of what "the keyboard does not work"
		// was.
		//
		// ON A ONE-SCREEN DEVICE THIS IS WRONG, and doing it there was a
		// regression found the first time the installer was run on an RP5.
		// A focused field makes SDL start text input, which makes ANDROID's
		// own keyboard open - so the login screen came up with half of itself
		// already covered, including the list of games to join, before the
		// player had asked to type anything. Tapping the box is the right way
		// in when there is a box you can reach.
		if (SecondScreen::available()
			&& !UI::get().has_focused_textfield()
			&& account.get_state() != Textfield::State::DISABLED
			&& password.get_state() != Textfield::State::DISABLED)
		{
			// Whichever is still empty. With a saved account name that is the
			// password, which is where somebody would have tapped anyway.
			if (account.empty())
				account.set_state(Textfield::State::FOCUSED);
			else
				password.set_state(Textfield::State::FOCUSED);
		}

		if (panel == Panel::CREATE || panel == Panel::HOST_NETWORK)
		{
			// Finishing the setup in Termux turns the warnings off while the
			// popup is still open, rather than needing a restart to notice.
			// Nothing is STARTED by this - only looked at.
			if (--until_refresh <= 0)
			{
				until_refresh = 30;
				readiness = LocalServer::check();
			}

			return;
		}

		// Reach for Wi-Fi Direct only when there is genuinely nothing else.
		//
		// An earlier version fell back on a timer alone and fired while
		// sitting on the house wifi with a host two feet away that had simply
		// not resolved yet. Making a network of our own in that situation is
		// wrong. No network AT ALL is the trigger; a slow one is not.
		if (!hosting && found.empty() && !tried_wifi_direct)
		{
			if (++looking_for > PATIENCE && !Multiplayer::on_network())
			{
				tried_wifi_direct = true;

				// With the radio off there is nothing to try, and saying so
				// beats a string of BUSY failures that look like broken
				// hardware.
				if (Multiplayer::wifi_direct_supported() && Multiplayer::wifi_radio_on())
					Multiplayer::find_groups();
			}
		}

		if (--until_refresh > 0)
			return;

		until_refresh = 30;

		if (hosting)
			return;

		found = Multiplayer::games();

		Multiplayer::refresh_role();

		// Once we are a client in somebody's Wi-Fi Direct group the host is
		// at a KNOWN address - a group owner is always 192.168.49.1. Offer it
		// by hand if discovery has not managed to name it: mDNS does not
		// reliably cross a p2p link, and a game you cannot see is worse than
		// one without a pretty name.
		//
		// It carries no code, because we never heard the announcement that
		// would have had one in it - so it joins openly, like an older host.
		if (found.empty() && Multiplayer::role() == Multiplayer::Role::CLIENT)
			found.push_back({ "Nearby game", Multiplayer::GROUP_OWNER, 0 });

	}

	Cursor::State UILogin::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// The server wait owns the screen above everything, including the
		// other popups - it is drawn last, so it must be tested first.
		if (waiting_for_server)
		{
			// Only pressable once it is on screen - see draw_server_wait.
			if (clicked && waited > 2500
				&& wait_dismiss_bounds().contains(cursorpos))
				waiting_for_server = false;

			return Cursor::State::IDLE;
		}

		// While a popup is up it owns the screen. Letting taps through to the
		// login fields behind it is how somebody ends up typing into a box
		// they cannot see.
		if (panel != Panel::NONE)
		{
			if (cancel_bounds().contains(cursorpos))
			{
				// BACK from the network choice returns to the code rather
				// than closing everything - it is one step of a flow, not a
				// separate errand, and the code typed is not thrown away.
				if (clicked)
				{
					if (panel == Panel::HOST_NETWORK)
						panel = Panel::CREATE;
					else
						close_panel();
				}

				return Cursor::State::CANCLICK;
			}

			if (panel == Panel::HOST_NETWORK)
			{
				bool ok[2] =
				{
					Multiplayer::on_network(),
					Multiplayer::wifi_direct_supported() && Multiplayer::wifi_radio_on()
				};

				for (int16_t i = 0; i < 2; i++)
				{
					if (!choice_bounds(i).contains(cursorpos))
						continue;

					// Each row IS the commit. There is no separate button,
					// because picking how and then confirming it would be one
					// step too many for a choice this plain.
					if (clicked && ok[i])
						commit_host(i == 1);

					return Cursor::State::CANCLICK;
				}

				return Cursor::State::IDLE;
			}

			if (commit_bounds().contains(cursorpos))
			{
				if (clicked && static_cast<int16_t>(typed.size()) == CODE_LEN)
				{
					// CREATE does not host yet - it asks HOW first.
					if (panel == Panel::CREATE)
					{
						if (readiness.can_try())
							panel = Panel::HOST_NETWORK;
					}
					else
					{
						commit_join();
					}
				}

				return Cursor::State::CANCLICK;
			}

			if (clicked && keypad_pressed(cursorpos))
				return Cursor::State::CANCLICK;

			return Cursor::State::IDLE;
		}

		if (Cursor::State new_state = account.send_cursor(cursorpos, clicked))
			return new_state;

		if (Cursor::State new_state = password.send_cursor(cursorpos, clicked))
			return new_state;

		if (hosting)
		{
			Rectangle<int16_t> stop = list_row(0);
			Rectangle<int16_t> at(
				Point<int16_t>(stop.left(), stop.top() + 78),
				Point<int16_t>(stop.right(), stop.bottom() + 78));

			if (at.contains(cursorpos))
			{
				// There has to be a way back, or a device that hosted once
				// can never join anybody - which is the deadlock this whole
				// screen was rebuilt to prevent.
				if (clicked)
					stop_hosting();

				return Cursor::State::CANCLICK;
			}

			return UIElement::send_cursor(clicked, cursorpos);
		}

		int16_t shown = create_row();

		for (int16_t i = 0; i < shown; i++)
		{
			if (!list_row(i).contains(cursorpos))
				continue;

			if (clicked)
			{
				target = found[i];
				picked = true;

				// A host with no code has nothing to check, so there is
				// nothing to ask and the game opens straight away.
				if (target.code)
				{
					open_panel(Panel::CODE);
				}
				else
				{
					typed.clear();
					commit_join();
				}
			}

			return Cursor::State::CANCLICK;
		}

		if (list_row(shown).contains(cursorpos))
		{
			if (clicked)
				open_panel(Panel::CREATE);

			return Cursor::State::CANCLICK;
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}

	void UILogin::login()
	{
		account.set_state(Textfield::State::DISABLED);
		password.set_state(Textfield::State::DISABLED);

		std::string account_text = account.get_text();
		std::string password_text = password.get_text();

		std::function<void()> okhandler = [&, password_text]()
		{
			account.set_state(Textfield::State::NORMAL);
			password.set_state(Textfield::State::NORMAL);

			if (!password_text.empty())
				password.set_state(Textfield::State::FOCUSED);
			else
				account.set_state(Textfield::State::FOCUSED);
		};

		// The client is allowed to open with nothing listening, so this is
		// where a missing server is first noticed - and it has to be said
		// plainly, naming the address, or "nothing happens" is all anyone
		// sees.
		if (!Session::get().is_connected() && !Session::get().reconnect_to_configured())
		{
			std::string where = LocalServer::is_offline()
				? std::string("this device")
				: LocalServer::home_address();

			// By value, like every other notice here. The dialog outlives this
			// function and calls back long after it has returned, so a
			// reference to a local would be dangling by then.
			UI::get().emplace<UIOk>(
				"No answer from " + where + ".\\nIs the server running? The SERVER switch is top left.",
				[okhandler](bool) { okhandler(); });

			return;
		}

		if (account_text.empty())
		{
			UI::get().emplace<UILoginNotice>(UILoginNotice::Message::NOT_REGISTERED, okhandler);
			return;
		}

		if (password_text.length() <= 4)
		{
			UI::get().emplace<UILoginNotice>(UILoginNotice::Message::WRONG_PASSWORD, okhandler);
			return;
		}

		// Remember both halves when the box is ticked, so logging in is one
		// tap. Only the account name was ever saved, even though the client
		// already reads a saved password back into the field at startup and a
		// SavePassword setting exists - the reading half was there and the
		// writing half was not.
		//
		// The password is stored as typed, in the settings file next to the
		// game data. That is what "save my login" means here, and the file
		// sits inside the app's own directory.
		if (saveid)
		{
			Setting<DefaultAccount>::get().save(account_text);
			Setting<DefaultPassword>::get().save(password_text);
			Setting<SavePassword>::get().save(true);
		}
		else
		{
			Setting<DefaultAccount>::get().save("");
			Setting<DefaultPassword>::get().save("");
			Setting<SavePassword>::get().save(false);
		}

		Setting<SaveLogin>::get().save(saveid);

		// Write the file now rather than at exit. Configuration saves itself
		// from its destructor, which only runs on a clean shutdown - and on a
		// handheld the app usually gets closed from outside, so nothing was
		// ever written and the details never came back.
		Configuration::get().save();

		UI::get().emplace<UILoginwait>(okhandler);

		auto loginwait = UI::get().get_element<UILoginwait>();

		if (loginwait && loginwait->is_active())
			LoginPacket(account_text, password_text).dispatch();
	}

	void UILogin::open_url(uint16_t id)
	{
		std::string url;

		switch (id)
		{
		case Buttons::BT_REGISTER:
			url = Configuration::get().get_joinlink();
			break;
		case Buttons::BT_HOMEPAGE:
			url = Configuration::get().get_website();
			break;
		case Buttons::BT_PASSLOST:
			url = Configuration::get().get_findpass();
			break;
		case Buttons::BT_IDLOST:
			url = Configuration::get().get_findid();
			break;
		default:
			return;
		}

		// TODO: (rich) fix
		//ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
	}

	Rectangle<int16_t> UILogin::wait_dismiss_bounds() const
	{
		// Measured from the same numbers draw_server_wait uses, so the button
		// cannot drift away from the box it is drawn in.
		constexpr int16_t W = 150;
		constexpr int16_t H = 28;

		int16_t mid = 800 / 2;
		int16_t top = 600 / 2 - 60;

		return Rectangle<int16_t>(
			Point<int16_t>(mid - W / 2, top + 128),
			Point<int16_t>(mid + W / 2, top + 128 + H));
	}

	void UILogin::draw_server_wait(Point<int16_t> screen) const
	{
		if (wait_title.get_text().empty())
		{
			wait_title = Text(Text::Font::A13M, Text::Alignment::CENTER,
				Color::Name::WHITE);
			wait_line = Text(Text::Font::A12M, Text::Alignment::CENTER,
				Color::Name::WHITE);
		}

		// THE WHOLE SCREEN, and dark enough that nothing behind it invites a
		// press. A wait you can see past is a wait people try to work around.
		GraphicsGL::get().drawrectangle(0, 0, screen.x(), screen.y(),
			0.0f, 0.0f, 0.0f, 0.80f);

		int16_t mid = static_cast<int16_t>(screen.x() / 2);
		int16_t top = static_cast<int16_t>(screen.y() / 2 - 60);

		GraphicsGL::get().drawrectangle(mid - 190, top, 380, 124,
			0.10f, 0.11f, 0.14f, 0.96f);

		wait_title.change_text("Starting the game server");
		wait_title.draw(Point<int16_t>(mid, top + 16));

		// SOMETHING MOVING. A still box for ten seconds reads as a hang, and
		// the one thing this screen must not look like is a crash.
		int16_t dots = static_cast<int16_t>((waited / 30) % 4);
		std::string ellipsis(dots, '.');

		wait_line.change_text("Please wait" + ellipsis);
		wait_line.draw(Point<int16_t>(mid, top + 46));

		// WAIT. The screen closes ITSELF the moment the server answers -
		// it is asking every two seconds, not counting down - so there is
		// nothing to press and nothing to decide.
		if (waited > 2500)
		{
			wait_line.change_text("The server is not starting.");
			wait_line.draw(Point<int16_t>(mid, top + 74));
		}
		else if (waited > 900)
		{
			wait_line.change_text("The database takes a moment on a cold start.");
			wait_line.draw(Point<int16_t>(mid, top + 74));
		}
		else
		{
			wait_line.change_text("This takes a few seconds.");
			wait_line.draw(Point<int16_t>(mid, top + 74));
		}

		wait_line.change_text("Log in will work the moment it answers.");
		wait_line.draw(Point<int16_t>(mid, top + 96));

		// NO "CARRY ON" BUTTON.
		//
		// It was a shrug: it let somebody past the screen into a login that
		// could not work, which is the very thing this exists to prevent.
		//
		// A way out appears ONLY once the wait has clearly failed - twenty
		// seconds, well past any real start - and then it says what is wrong
		// rather than inviting you to ignore it. Waiting is not a decision
		// the player should be asked to make; giving up is.
		if (waited > 2500)
		{
			Rectangle<int16_t> out = wait_dismiss_bounds();

			GraphicsGL::get().drawrectangle(
				out.left(), out.top(), out.width(), out.height(),
				0.48f, 0.18f, 0.18f, 0.95f);

			wait_line.change_text("Give up and go back");
			wait_line.draw(Point<int16_t>(mid, out.top() + 5));
		}
	}

	Button::State UILogin::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case Buttons::BT_LOGIN:
			// Not while the server we just started is still coming up. The
			// wait screen is over the top of this anyway; refusing here means
			// a stray tap that lands before it draws cannot get through
			// either.
			if (waiting_for_server)
				return Button::State::NORMAL;

			login();

			return Button::State::NORMAL;
		case Buttons::BT_REGISTER:
		case Buttons::BT_HOMEPAGE:
		case Buttons::BT_PASSLOST:
		case Buttons::BT_IDLOST:
			open_url(id);

			return Button::State::NORMAL;
		case Buttons::BT_SAVEID:
			saveid = !saveid;
			Setting<SaveLogin>::get().save(saveid);

			return Button::State::MOUSEOVER;
		case Buttons::BT_QUIT:
			UI::get().quit();

			return Button::State::PRESSED;
		default:
			return Button::State::NORMAL;
		}
	}

	UIElement::Type UILogin::get_type() const
	{
		return TYPE;
	}
}
