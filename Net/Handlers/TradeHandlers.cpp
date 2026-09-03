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
#include "TradeHandlers.h"

#include "Helpers/ItemParser.h"
#include "Helpers/LoginParser.h"

#include "../Packets/TradePackets.h"

#include "../../IO/UI.h"
#include "../../IO/SecondScreen.h"
#include "../../IO/UITypes/UINotice.h"
#include "../../IO/UITypes/UITrade.h"

#include "../../Util/Silent.h"

namespace ms
{
	namespace
	{
		// WHERE THE TRADE WINDOW LIVES.
		//
		// On the lower panel where there is one, because that is where every
		// other window in this build goes and because trading with a thumb on
		// the top screen means covering the game with it.
		//
		// On a one-screen device it is an ordinary window over the game. Both
		// are the same class; only who owns it differs.
		// `turn_to_it` is the difference between the two packets that BEGIN a
		// trade and the dozen that follow.
		//
		// Opening one is worth taking over the screen for - somebody just
		// agreed to trade and the table is what they want to see. An item
		// landing on it is not: if this turned the page every time, a player
		// who wandered off to check their inventory mid-trade would be
		// dragged back every time their partner put something down.
		UITrade* trade_window(bool turn_to_it)
		{
			if (SecondScreen::available())
			{
				UIElement* page = turn_to_it
					? SecondScreen::open_trade()
					: SecondScreen::hosted(UIElement::Type::TRADE);

				return static_cast<UITrade*>(page);
			}

			if (auto existing = UI::get().get_element<UITrade>())
				return existing.get();

			if (!turn_to_it)
				return nullptr;

			UI::get().emplace<UITrade>();

			auto made = UI::get().get_element<UITrade>();

			return made ? made.get() : nullptr;
		}

		// The character look is read and thrown away.
		//
		// The window draws names and items, not people - there is no room on
		// a 344-wide panel for two paper dolls, and a name is what tells you
		// who you are trading with anyway. But the bytes are in the packet
		// and everything after them depends on the stream being in the right
		// place, so they have to be READ rather than assumed to be some fixed
		// length. LoginParser::parse_look already knows the layout.
		void skip_look(InPacket& recv)
		{
			LoginParser::parse_look(recv);
		}
	}

	void PlayerInteractionHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();

		switch (mode)
		{
		case TradeAction::INVITE:
		{
			// byte 3 (the room type), the inviter's name, then four bytes
			// Cosmic writes as a constant and nobody reads.
			recv.read_byte();

			std::string from = recv.read_string();

			// CONSUME THEM ANYWAY.
			//
			// PacketCreator.tradeInvite ends with a fixed B7 50 00 00 and
			// nothing needs the value - but a handler that stops early makes
			// PacketSwitch log "opcode 314 left 4 bytes unread" every single
			// time. That noise is not free: the client log is how a fault is
			// found now, and a line that always appears is a line nobody
			// reads.
			recv.skip(4);

			// ASKED, NOT TOLD. Accepting has to be a decision - a trade
			// window that simply appears while somebody is fighting is a
			// window they will close by pressing whatever is under their
			// thumb.
			UI::get().emplace<UIYesNo>(
				from + " wants to trade with you.",
				[](bool yes)
				{
					if (yes)
						TradeAcceptPacket().dispatch();
					else
						TradeDeclinePacket().dispatch();
				});

			break;
		}
		case TradeAction::ROOM:
		{
			int8_t type = recv.read_byte();

			if (type != 3)
			{
				// A player shop or a minigame. Not built yet, and saying so
				// beats a window that never opens.
				Silent::report("PlayerInteractionHandler",
					"room type " + std::to_string(type) + " is not a trade");

				break;
			}

			recv.read_byte();               // max people, always 2
			int8_t number = recv.read_byte();

			std::string partner;

			// The partner block is only there for whoever ACCEPTED - the
			// inviter is shown an empty table and told who joined later.
			if (number == 1)
			{
				recv.read_byte();
				skip_look(recv);

				partner = recv.read_string();
			}

			recv.read_byte();               // our own number again
			skip_look(recv);
			recv.read_string();             // our own name

			// getTradeStart signs off with 0xFF. Reading it keeps the packet
			// balanced - this is the "opcode 314 left 1 bytes unread" that
			// showed up on every single trade attempt.
			recv.read_byte();

			if (UITrade* trade = trade_window(true))
				trade->opened(number, partner);

			break;
		}
		case TradeAction::VISIT:
		{
			// Somebody walked in: the answer to an invitation we sent.
			recv.read_byte();               // which side, always 1 here
			skip_look(recv);

			std::string who = recv.read_string();

			if (UITrade* trade = trade_window(true))
				trade->partner_joined(who);

			break;
		}
		case TradeAction::SET_ITEMS:
		{
			int8_t whose = recv.read_byte();
			int8_t slot = recv.read_byte();

			ItemParser::Skimmed item = ItemParser::skim_item(recv);

			if (UITrade* trade = trade_window(false))
				trade->put_item(whose, slot, item.id, item.count);

			break;
		}
		case TradeAction::SET_MESO:
		{
			int8_t whose = recv.read_byte();
			int32_t meso = recv.read_int();

			if (UITrade* trade = trade_window(false))
				trade->put_meso(whose, meso);

			break;
		}
		case TradeAction::CONFIRM:
		{
			// No body. It always means THEM: the server sends it to the
			// partner of whoever locked in.
			if (UITrade* trade = trade_window(false))
				trade->partner_confirmed();

			break;
		}
		case TradeAction::CHAT:
		{
			recv.read_byte();               // CHAT_THING again
			recv.read_byte();               // 0 if we said it, 1 if they did

			std::string line = recv.read_string();

			if (UITrade* trade = trade_window(false))
				trade->said(line);

			break;
		}
		case TradeAction::EXIT:
		{
			// EXIT IS TWO PACKETS WEARING ONE NUMBER.
			//
			// A trade ending carries our side and the reason. A shop closing
			// carries a slot, or nothing at all. Length is the only thing
			// that separates them, which is exactly the trap this project has
			// hit before on the server side - so it is read as a length here
			// rather than assumed.
			// `length()`, NOT `available()`.
			//
			// available() returns a BOOL - "is there anything left" - so
			// `available() < 2` compares true against 2 and is ALWAYS true.
			// This branch therefore broke out every single time without
			// reading anything, which meant the end of a trade was never
			// processed at all: no result, no closed window, and an inviter
			// left looking at "waiting" for ever. The two unread bytes it
			// left behind were the only trace, and they said "opcode 314"
			// rather than which of its twenty meanings had gone wrong.
			if (recv.length() < 2)
				break;

			recv.read_byte();               // our number
			int8_t operation = recv.read_byte();

			// The ending IS worth turning to. It carries the only report
			// anybody gets of whether the trade went through.
			if (UITrade* trade = trade_window(true))
				trade->closed(operation);

			break;
		}
		default:
			// Player shops, hired merchants, omok, match cards. Named rather
			// than dropped: an unhandled branch that is silent looks exactly
			// like a server that never sent anything.
			Silent::report("PlayerInteractionHandler",
				"unhandled action " + std::to_string(static_cast<int32_t>(mode)));

			break;
		}

		// WHICH trade packet did not add up.
		//
		// PacketSwitch can only say "opcode 314 left 2 bytes unread", and 314
		// is twenty different messages wearing one number - so that line
		// names the street and not the house. Two rounds of guessing which
		// mode it was is two rounds too many: the handler knows, so it says.
		if (size_t over = recv.length())
			Silent::report("PlayerInteractionHandler",
				"action " + std::to_string(static_cast<int32_t>(mode))
				+ " left " + std::to_string(over) + " bytes unread");
	}
}
