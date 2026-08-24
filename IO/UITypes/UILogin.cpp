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
#include "../../Net/Session.h"
#include "UILoginwait.h"
#include "UILoginNotice.h"
#include "UINotice.h"

#include "../UI.h"

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
	}

	UILogin::UILogin() : UIElement(Point<int16_t>(0, 0), Point<int16_t>(800, 600))
	{
		Music("BgmUI.img/Title").play();

		std::string version_text = Configuration::get().get_version();
		mode_label = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE);
		mode_hint = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::LIGHTGREY);
		game_name = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);
		check_line = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);

		// Hosting is the default, because it is also what playing alone is.
		choose_host();

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
			sprites.emplace_back(custom["LoginBg"], DrawArgument(Point<int16_t>(0, 0), Point<int16_t>(800, 600)));

			sprites.emplace_back(custom["Logo"], LOGO_POS);
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

		version.draw(position + Point<int16_t>(707, 1));

		// HOST and JOIN, top-left where nothing else lives and the mount is
		// not in the way.
		for (Mode m : { Mode::HOST, Mode::JOIN })
		{
			Rectangle<int16_t> at = mode_bounds(m);
			bool here = (m == mode);

			GraphicsGL::get().drawrectangle(
				at.left(), at.top(), at.width(), at.height(),
				here ? 0.20f : 0.10f,
				here ? 0.30f : 0.12f,
				here ? 0.24f : 0.16f, 0.92f);

			mode_label.change_text(m == Mode::HOST ? "HOST" : "JOIN");
			mode_label.draw(Point<int16_t>(at.left() + at.width() / 2, at.top() + 3));
		}

		Rectangle<int16_t> row = mode_bounds(Mode::JOIN);

		mode_hint.change_text(mode == Mode::HOST
			? "Play solo, or host a play session"
			: "Join a host's play session");

		mode_hint.draw(Point<int16_t>(mode_bounds(Mode::HOST).left(), row.bottom() + 3));

		// Under HOST: what is ready and what is not. Somebody setting a
		// device up for the first time should be able to SEE what is left,
		// rather than tapping HOST and being told one vague thing.
		if (mode == Mode::HOST)
		{
			struct { bool ok; const char* label; const char* fix; } lines[] =
			{
				{ readiness.termux,      "Termux installed",  "install it - see docs_OFFLINE.md" },
				{ readiness.permission,  "May start the server", "tap HOST again and allow it" },
				{ readiness.server,      "Game server set up", "run termux_setup.sh once" },
				{ readiness.wifi_direct, "Can make its own network", "not needed if there is wifi" }
			};

			int16_t y = row.bottom() + 21;

			for (const auto& line : lines)
			{
				// A tick or a cross, then what it is. The last one is not
				// required, so it is never a hard failure - it only decides
				// whether a car with no router will work.
				check_line.change_text(std::string(line.ok ? "[+] " : "[X] ") + line.label);
				check_line.draw(Point<int16_t>(mode_bounds(Mode::HOST).left() + 4, y));
				y += 15;

				if (!line.ok)
				{
					check_line.change_text(std::string("      ") + line.fix);
					check_line.draw(Point<int16_t>(mode_bounds(Mode::HOST).left() + 4, y));
					y += 15;
				}
			}
		}

		// The list of games, by NAME. Only while JOIN is chosen - browsing
		// costs battery and there is nothing to show otherwise.
		if (mode == Mode::JOIN)
		{
			if (found.empty())
			{
				game_name.change_text("Looking for games...");
				game_name.draw(Point<int16_t>(row.left() + 4, row.bottom() + 21));
			}
			else
			{
				for (size_t i = 0; i < found.size() && i < 5; i++)
				{
					Rectangle<int16_t> at = game_bounds(static_cast<int16_t>(i));
					bool chosen = found[i].address == Setting<ServerIP>::get().load();

					GraphicsGL::get().drawrectangle(
						at.left(), at.top(), at.width(), at.height() - 2,
						chosen ? 0.20f : 0.08f,
						chosen ? 0.34f : 0.09f,
						chosen ? 0.24f : 0.12f, 0.9f);

					game_name.change_text(found[i].name);
					game_name.draw(Point<int16_t>(at.left() + 6, at.top() + 2));
				}
			}
		}
		account.draw(position);
		password.draw(position);

		if (account.get_state() == Textfield::State::NORMAL && account.empty())
			accountbg.draw(DrawArgument(position + Point<int16_t>(291, 279) + PANEL));

		if (password.get_state() == Textfield::State::NORMAL && password.empty())
			passwordbg.draw(DrawArgument(position + Point<int16_t>(291, 305) + PANEL));

		checkbox[saveid].draw(DrawArgument(position + Point<int16_t>(291, 335) + PANEL));
	}

	Rectangle<int16_t> UILogin::mode_bounds(Mode which) const
	{
		constexpr int16_t W = 86;
		constexpr int16_t H = 22;

		int16_t x = 8 + (which == Mode::JOIN ? W + 4 : 0);

		Point<int16_t> at = position + Point<int16_t>(x, 6);

		return Rectangle<int16_t>(at, at + Point<int16_t>(W, H));
	}

	Rectangle<int16_t> UILogin::game_bounds(int16_t row) const
	{
		constexpr int16_t H = 20;

		Point<int16_t> at = position
			+ Point<int16_t>(8, 6 + 22 + 18 + row * H);

		return Rectangle<int16_t>(at, at + Point<int16_t>(176, H));
	}

	void UILogin::choose_host()
	{
		mode = Mode::HOST;

		Multiplayer::stop_browsing();
		found.clear();

		readiness = LocalServer::check();

		// Nothing else here can work until the device is set up, and saying
		// so as a list beats failing later with one message.
		if (!readiness.ready())
			return;

		// Hosting means playing against the server on this device, whoever
		// else joins - so the client points at its own loopback either way.
		LocalServer::set_offline(true);

		if (!LocalServer::can_host())
		{
			// Termux is how the server runs, and it is not here yet. Say so
			// once, plainly, rather than failing at the login.
			UI::get().emplace<UIOk>(
				"There is no server on this device yet.\\nIt needs Termux installed and set up once - see docs_OFFLINE.md.",
				[](bool) {});

			return;
		}

		LocalServer::start();

		// Shout our name onto the network so the others can find us without
		// anybody reading out an address.
		Multiplayer::start_hosting(Multiplayer::suggested_name());
	}

	void UILogin::choose_join()
	{
		mode = Mode::JOIN;

		// Stop being a host: two servers announcing themselves on one network
		// is how a child ends up joining their own device.
		Multiplayer::stop_hosting();
		LocalServer::set_offline(false);

		found.clear();
		until_refresh = 1;

		Multiplayer::start_browsing();
	}

	void UILogin::update()
	{
		UIElement::update();

		account.update(position);
		password.update(position);

		// While the list is showing, ask Android what it has found. The
		// answer changes on its threads, so it is polled here rather than
		// pushed - about twice a second, which is faster than anybody can
		// read a new name appearing.
		if (--until_refresh <= 0)
		{
			until_refresh = 30;

			if (mode == Mode::JOIN)
			{
				found = Multiplayer::games();
			}
			else
			{
				// So that finishing the setup in Termux turns the list green
				// while the game is still open, rather than needing a
				// restart to notice.
				bool was_ready = readiness.ready();
				readiness = LocalServer::check();

				if (!was_ready && readiness.ready())
					choose_host();
			}
		}
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

	Button::State UILogin::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case Buttons::BT_LOGIN:
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

	Cursor::State UILogin::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (Cursor::State new_state = account.send_cursor(cursorpos, clicked))
			return new_state;

		if (Cursor::State new_state = password.send_cursor(cursorpos, clicked))
			return new_state;

		for (Mode m : { Mode::HOST, Mode::JOIN })
		{
			if (!mode_bounds(m).contains(cursorpos))
				continue;

			if (clicked && mode != m)
			{
				if (m == Mode::HOST)
					choose_host();
				else
					choose_join();
			}

			return Cursor::State::CANCLICK;
		}

		// Picking a game out of the list. A NAME is tapped; the address goes
		// quietly into the settings and is never shown.
		if (mode == Mode::JOIN)
		{
			for (size_t i = 0; i < found.size() && i < 5; i++)
			{
				if (!game_bounds(static_cast<int16_t>(i)).contains(cursorpos))
					continue;

				if (clicked)
				{
					Setting<ServerIP>::get().save(found[i].address);
					Configuration::get().save();
				}

				return Cursor::State::CANCLICK;
			}
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}

	UIElement::Type UILogin::get_type() const
	{
		return TYPE;
	}
}
