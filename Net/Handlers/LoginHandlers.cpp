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
#include "LoginHandlers.h"

#include "Helpers/LoginParser.h"

#include "../Session.h"

#include "../../Util/Silent.h"
#include "../../Util/Carry.h"
#include "../../Util/PostBox.h"

#include "../Packets/LoginPackets.h"

#include "../../Gameplay/Stage.h"
#include "../IO/UI.h"

#include "../IO/UITypes/UILoginNotice.h"
#include "../IO/UITypes/UIWorldSelect.h"
#include "../IO/UITypes/UICharSelect.h"
#include "../IO/UITypes/UIRaceSelect.h"
#include "../IO/UITypes/UILoginwait.h"
#include "../IO/UITypes/UITermsOfService.h"
#include "../IO/UITypes/UIGender.h"

namespace ms
{
	void LoginResultHandler::handle(InPacket& recv) const
	{
		auto loginwait = UI::get().get_element<UILoginwait>();

		if (loginwait && loginwait->is_active())
		{
			// Remove previous UIs.
			UI::get().remove(UIElement::Type::LOGINNOTICE);
			UI::get().remove(UIElement::Type::LOGINWAIT);
			UI::get().remove(UIElement::Type::TOS);
			UI::get().remove(UIElement::Type::GENDER);

			std::function<void()> okhandler = loginwait->get_handler();

			// The packet should contain a 'reason' integer which can signify various things.
			if (int32_t reason = recv.read_int())
			{
				// Login unsuccessfull. The LoginNotice displayed will contain the specific information.
				switch (reason)
				{
				case 2:
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::BLOCKED_ID, okhandler);
					break;
				case 5:
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::NOT_REGISTERED, okhandler);
					break;
				case 7:
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::ALREADY_LOGGED_IN, okhandler);
					break;
				case 13:
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::UNABLE_TO_LOGIN_WITH_IP, okhandler);
					break;
				case 23:
					// The server sends a request to accept the terms of service.
					UI::get().emplace<UITermsOfService>(okhandler);
					break;
				default:
					// Other reasons.
					if (reason > 0)
					{
						auto reasonbyte = static_cast<int8_t>(reason - 1);

						UI::get().emplace<UILoginNotice>(reasonbyte, okhandler);
					}
				}
			}
			else
			{
				// Login successfull. The packet contains information on the account, so we initialise the account with it.
				Account account = LoginParser::parse_account(recv);

				// WHOSE CHARACTERS THESE ARE. The card is written per account,
				// and this is the first moment the server has confirmed the
				// name rather than the login box merely holding one.
				Carry::get().set_account(account.name);

				// A NAME TO BE GOING ON WITH, replaced by the character's own
				// as soon as one is in the world - see PostBox::update. The
				// account name is not visible to anybody else, so it cannot
				// be what messages are addressed to.
				PostBox::get().set_account(account.name);

				if (account.female == 10)
				{
					UI::get().emplace<UIGender>(okhandler);
				}
				else
				{
					// Save the Login ID if the box for it on the login panel is checked.
					if (Setting<SaveLogin>::get().load()) {
						Setting<DefaultAccount>::get().save(account.name);
					}

					//AfterLoginPacket("1111").dispatch();

					// Request the list of worlds and channels online.
					ServerRequestPacket().dispatch();
				}
			}
		}
	}

	void ServerlistHandler::handle(InPacket& recv) const
	{
		auto worldselect = UI::get().get_element<UIWorldSelect>();

		if (!worldselect)
			worldselect = UI::get().emplace<UIWorldSelect>();

		// Parse all worlds.
		while (recv.available())
		{
			World world = LoginParser::parse_world(recv);

			if (world.wid != -1)
			{
				worldselect->add_world(world);
			}
			else
			{
				// Remove previous UIs.
				UI::get().remove(UIElement::Type::LOGIN);

				// ONE WORLD IS NOT A CHOICE.
				//
				// This build runs one world with one channel, on somebody's
				// handheld in their own house. Asking which of the one worlds
				// they would like, and then which of its one channels, is two
				// screens that can only be answered one way - and both of them
				// stand between a child and the game.
				//
				// Skipped only when there really IS just one. If a second ever
				// appears the screen comes back by itself, with no flag to
				// remember to turn off.
				if (worldselect->only_one_world())
				{
					worldselect->enter_only_world();

					return;
				}

				// Add the world selection screen to the ui.
				worldselect->draw_world();

				// "End of serverlist" packet.
				return;
			}
		}
	}

	void RecommendedWorldsHandler::handle(InPacket& recv) const
	{
		if (auto worldselect = UI::get().get_element<UIWorldSelect>())
		{
			int16_t count = recv.read_byte();

			for (size_t i = 0; i < count; i++)
			{
				RecommendedWorld world = LoginParser::parse_recommended_world(recv);

				if (world.wid != -1 && !world.message.empty())
					worldselect->add_recommended_world(world);
			}
		}
	}

	void CharlistHandler::handle(InPacket& recv) const
	{
		auto loginwait = UI::get().get_element<UILoginwait>();

		if (loginwait && loginwait->is_active())
		{
			uint8_t channel_id = recv.read_byte();

			// Parse all characters.
			std::vector<CharEntry> characters;
			int8_t charcount = recv.read_byte();

			for (uint8_t i = 0; i < charcount; ++i)
				characters.emplace_back(LoginParser::parse_charentry(recv));

			int8_t pic = recv.read_byte();
			int32_t slots = recv.read_int();

			// Remove previous UIs.
			UI::get().remove(UIElement::Type::LOGINNOTICE);
			UI::get().remove(UIElement::Type::LOGINWAIT);

			// Remove the world selection screen.
			if (auto worldselect = UI::get().get_element<UIWorldSelect>())
				worldselect->remove_selected();

			// Add the character selection screen.
			UI::get().emplace<UICharSelect>(characters, charcount, slots, pic);
		}
	}

	void CharnameResponseHandler::handle(InPacket& recv) const
	{
		// Read the name and if it is already in use.
		std::string name = recv.read_string();
		bool used = recv.read_bool();

		// Notify the character creation screen.
		if (auto raceselect = UI::get().get_element<UIRaceSelect>())
			raceselect->send_naming_result(used);
	}

	void AddNewCharEntryHandler::handle(InPacket& recv) const
	{
		recv.skip(1);

		// Parse info on the new character.
		CharEntry character = LoginParser::parse_charentry(recv);

		// Read the updated character selection.
		if (auto charselect = UI::get().get_element<UICharSelect>())
			charselect->add_character(std::move(character));
	}

	void DeleteCharResponseHandler::handle(InPacket& recv) const
	{
		// Read the character id and if deletion was successfull (pic was correct).
		int32_t cid = recv.read_int();
		uint8_t state = recv.read_byte();

		// Extract information from the state byte.
		if (state)
		{
			UILoginNotice::Message message;

			switch (state)
			{
			case 10:
				message = UILoginNotice::Message::BIRTHDAY_INCORRECT;
				break;
			case 20:
				message = UILoginNotice::Message::INCORRECT_PIC;
				break;
			default:
				message = UILoginNotice::Message::UNKNOWN_ERROR;
			}

			UI::get().emplace<UILoginNotice>(message);
		}
		else
		{
			if (auto charselect = UI::get().get_element<UICharSelect>())
				charselect->remove_character(cid);
		}
	}

	void ServerIPHandler::handle(InPacket& recv) const
	{
		recv.skip(2);

		// Read the ipv4 address in a string.
		std::string addrstr;

		for (int i = 0; i < 4; i++)
		{
			uint8_t num = static_cast<uint8_t>(recv.read_byte());
			addrstr.append(std::to_string(num));

			if (i < 3)
				addrstr.push_back('.');
		}

		// Read the port address in a string.
		std::string portstr = std::to_string(recv.read_short());

		int32_t cid = recv.read_int();

		// PRESSING START AND NOTHING HAPPENING.
		//
		// The address here is not the one the player typed - it is whatever
		// the SERVER says its channel lives at, from `HOST` in its config.
		// On a handheld host that value goes stale every time the router
		// hands out a new address, and then everyone except the host itself
		// is sent somewhere with nothing on it.
		//
		// This used to reconnect, ignore the answer, and dispatch the login
		// packet into a dead socket. No message, no log line, no way to tell
		// it apart from a button that does not work - which is exactly what
		// it was reported as.
		if (!Session::get().reconnect(addrstr.c_str(), portstr.c_str()))
		{
			std::string where = addrstr + ":" + portstr;

			Silent::report("ServerIPHandler",
				"could not reach the game world at " + where);

			// ON THE SCREEN, not just in a log. The person who meets this is
			// holding the handheld and cannot read a log. The exact address
			// is the diagnosis and it goes to playlog.txt above, where it can
			// be read back afterwards.
			UI::get().emplace<UILoginNotice>(
				UILoginNotice::Message::UNABLE_TO_CONNECT);

			// BACK TO A SCREEN THAT WORKS. Without this the character select
			// sits there disabled behind the notice, because the click that
			// started all this called UI::disable().
			UI::get().enable();

			return;
		}

		PlayerLoginPacket(cid).dispatch();
	}

	void ChangeChannelHandler::handle(InPacket& recv) const
	{
		recv.read_byte(); // always 1

		std::string addrstr;

		for (int i = 0; i < 4; i++)
		{
			uint8_t num = static_cast<uint8_t>(recv.read_byte());
			addrstr.append(std::to_string(num));

			if (i < 3)
				addrstr.push_back('.');
		}

		std::string portstr = std::to_string(recv.read_short());

		// No character id is sent - the server takes it that we still know
		// who we are, and we do: the player survives Stage::clear().
		int32_t cid = Stage::get().get_player().get_oid();

		// THE WORLD WE WERE LOOKING AT IS GONE.
		//
		// A channel change is how the cash shop is left, and this handler
		// never cleared the map - so Amherst's NPCs came back out of the shop
		// with the player and stood about in whatever map they landed in.
		// Pio and Rain exist in exactly one map in the whole game, and they
		// were on screen in Dangerous Forest.
		//
		// The comment here already said the player "survives Stage::clear()",
		// which is only worth saying if a clear was meant to be happening.
		// It was not.
		//
		// Before the reconnect, so the tidy-up packet it sends still has a
		// connection to go out on.
		Stage::get().clear();

		if (!Session::get().reconnect(addrstr.c_str(), portstr.c_str()))
		{
			std::string where = addrstr + ":" + portstr;

			// Same fault as ServerIPHandler had: the answer was thrown away
			// and the login packet sent into a dead socket, leaving the
			// player on a blank screen with nothing said.
			Silent::report("ChangeChannelHandler",
				"could not reach the world again at " + where);

			UI::get().emplace<UILoginNotice>(
				UILoginNotice::Message::UNABLE_TO_CONNECT);

			UI::get().enable();

			return;
		}

		PlayerLoginPacket(cid).dispatch();
	}
}