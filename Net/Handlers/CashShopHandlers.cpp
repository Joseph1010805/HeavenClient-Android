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
#include "CashShopHandlers.h"

#include "../PacketError.h"

#include "../../Timer.h"

#include "../../IO/UITypes/UIStatusMessenger.h"

#include "Helpers/CashShopParser.h"

#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIChatBar.h"
#include "../../IO/UITypes/UICashShop.h"
#include "../../IO/UITypes/UIStatusMessenger.h"
#include "../../IO/Window.h"

namespace ms
{
	void SetCashShopHandler::handle(InPacket& recv) const
	{
		// The character block is read for what it updates, but a failure here
		// must NOT stop the screen changing.
		//
		// By the time this arrives the server has already removed the
		// character from the channel and from the map. Throwing out of this
		// handler does not mean "the shop did not open" - it means the player
		// is left looking at a world they are no longer standing in, unable
		// to walk or use a portal. Arriving at the shop with stale stats is
		// recoverable; being stranded is not.
		try
		{
			CashShopParser::parseCharacterInfo(recv);
		}
		catch (const PacketError&)
		{
			// Deliberately swallowed. Whatever went unread only leaves stats
			// stale for a screen that shows none of them.
		}

		// The rest is the shop's own catalogue - a special-item list, the
		// most-seller tables for eight tabs, and padding. This client reads
		// none of it, so it is not read at all: keeping byte counts in step
		// with a server layout for data we discard buys nothing and is one
		// more thing that can throw.
		transition();
	}

	static std::vector<CashLockerItem> s_locker;
	static int64_t s_pending_take = 0;

	const std::vector<CashLockerItem>& get_cash_locker()
	{
		return s_locker;
	}

	void set_pending_cash_take(int64_t cashid)
	{
		s_pending_take = cashid;
	}

	// One locker record, as PacketCreator::addCashItemInformation writes it
	// for a non-gift item: 55 bytes, of which three matter here.
	static CashLockerItem read_locker_item(InPacket& recv)
	{
		CashLockerItem item;

		item.cashid = recv.read_long();
		recv.skip(4);					// account id
		recv.skip(4);					// always 0
		item.itemid = recv.read_int();
		recv.skip(4);					// commodity SN
		item.quantity = recv.read_short();
		recv.skip(13);					// gift sender, right-padded
		recv.skip(8);					// expiration
		recv.skip(8);					// always 0

		return item;
	}

	void CashShopOperationHandler::handle(InPacket& recv) const
	{
		int8_t operation = recv.read_byte();

		// These sub-opcodes come from Cosmic's PacketCreator, not from a v83
		// opcode list. The pair that used to be here - 0x4A for success and
		// 0x4C for failure - are not what this server sends, so a purchase
		// that worked and a purchase that was refused looked identical from
		// the outside: nothing at all.
		switch (operation)
		{
		case 0x4B:	// showCashInventory - the locker, sent on entering
		{
			s_locker.clear();

			int16_t count = recv.read_short();

			for (int16_t i = 0; i < count; i++)
				s_locker.push_back(read_locker_item(recv));

			// Storage slots and character slots follow; neither is shown.
			break;
		}
		case 0x57:	// showBoughtCashItem
		{
			s_locker.push_back(read_locker_item(recv));

			if (auto shop = UI::get().get_element<UICashShop>())
				shop->show_message(Color::Name::YELLOW, "Bought. Tap it in MY CASH ITEMS to take it out.");
			break;
		}
		case 0x68:	// takeFromCashInventory - it is on the character now
		{
			// The item's own data follows, but there is no need to read it:
			// the character's full inventory arrives with the map on the way
			// out of the shop. Only the locker needs correcting, and the
			// reply does not name the item - so the one that was asked for
			// is remembered on the way out instead.
			for (auto it = s_locker.begin(); it != s_locker.end(); ++it)
			{
				if (it->cashid != s_pending_take)
					continue;

				s_locker.erase(it);
				break;
			}

			s_pending_take = 0;

			if (auto shop = UI::get().get_element<UICashShop>())
				shop->show_message(Color::Name::YELLOW, "Taken out. It is in your inventory.");
			break;
		}
		case 0x59:
		{
			if (auto shop = UI::get().get_element<UICashShop>())
				shop->show_message(Color::Name::YELLOW, "Coupon redeemed.");
			break;
		}
		case 0x5C:	// showCashShopMessage - one byte, meaning per PacketCreator
		{
			uint8_t reason = static_cast<uint8_t>(recv.read_byte());
			std::string msg;

			switch (reason)
			{
			case 0xA8: msg = "You cannot send a gift to yourself."; break;
			case 0xA9: msg = "That character does not exist."; break;
			case 0xB8: msg = "This item cannot be used by your character."; break;
			case 0xBB: msg = "Your cash inventory is full."; break;
			case 0xBF: msg = "That item is not for sale right now."; break;
			case 0xC0: msg = "That item is out of stock."; break;
			case 0xC1: msg = "You have exceeded your NX spending limit."; break;
			case 0xC4: msg = "Check your birthday code."; break;
			case 0xCD: msg = "You have reached the daily purchase limit."; break;
			case 0xE6: msg = "That item cannot be bought with Maple Points."; break;
			default: msg = "The cash shop refused that (code 0x" + std::to_string(reason) + ")."; break;
			}

			if (auto shop = UI::get().get_element<UICashShop>())
				shop->show_message(Color::Name::RED, msg);
			break;
		}
		default:
			break;
		}
	}

	void SetITCHandler::handle(InPacket& recv) const
	{
		// SET_ITC — MTS transition
		// Same as SET_CASH_SHOP but with MTS trailing data instead of CS items
		CashShopParser::parseCharacterInfo(recv);

		recv.skip_string();	// account_name

		// MTS config bytes (hardcoded by Cosmic)
		recv.skip(28);

		// The MTS is deliberately not built - it is item trading between
		// accounts, which needs a market this server does not have. The
		// packet is still read to the end so it cannot be mistaken for an
		// unhandled one.
	}

	// Holds the UI scale that was active immediately before the cash shop
	// transition forced scale=1.0. UICashShop reads this back via
	// get_pre_cashshop_ui_scale() when the player leaves the shop.
	static float s_pre_cashshop_ui_scale = 1.0f;

	float get_pre_cashshop_ui_scale()
	{
		return s_pre_cashshop_ui_scale;
	}

	void SetCashShopHandler::transition() const
	{
		// OpenStory rescales the UI here, because its cash shop artwork is
		// authored 1:1 for a 1024x768 window. This client has no UI scaling
		// at all: the whole game is drawn into an 800x600 buffer and blitted
		// to whatever the screen is, so there is nothing to set and the shop
		// is stretched by the same blit as everything else.

		// Drop any focused textfield (e.g. the chat bar) so the arrow
		// keys reach the cash shop stage character
		UI::get().remove_textfield();

		Constants::Constants::get().set_viewwidth(1024);
		Constants::Constants::get().set_viewheight(768);

		float fadestep = 0.025f;

		Window::get().fadeout(
			fadestep,
			[]()
			{
				GraphicsGL::get().clear();

				// No map is loaded. OpenStory asks for map -1 here, meaning
				// "nowhere", but ours takes that literally: load_map(-1)
				// builds a name out of a negative id, finds no such node, and
				// puts together a map with no footholds and no portals - and
				// then respawn() goes looking for the portal to stand on.
				//
				// Nothing needs it. UIStateCashShop draws its elements and
				// nothing else; there is no world behind the shop.

				GraphicsGL::get().unlock();

				UI::get().change_state(UI::State::CASHSHOP);
				UI::get().enable();
				Timer::get().start();
			}
		);

		GraphicsGL::get().lock();
		Stage::get().clear();
		Timer::get().start();
	}

	static int32_t s_cash_balances[3] = { 0, 0, 0 };

	int32_t get_cash_balance(int which)
	{
		return (which >= 0 && which < 3) ? s_cash_balances[which] : 0;
	}

	void QueryCashResultHandler::handle(InPacket& recv) const
	{
		// Cash query result — the balances render in the cash shop's
		// left panel next to the baked NX labels
		if (recv.available())
		{
			s_cash_balances[0] = recv.read_int(); // NX credit
			s_cash_balances[1] = recv.read_int(); // maple points
			s_cash_balances[2] = recv.read_int(); // NX prepaid
		}
	}

	void CashShopNameChangeHandler::handle(InPacket& recv) const
	{
		std::string name = recv.read_string();
		bool in_use = recv.read_bool();

		if (auto messenger = UI::get().get_element<UIStatusMessenger>())
		{
			if (in_use)
				messenger->show_status(Color::Name::RED, "Name '" + name + "' is already in use.");
			else
				messenger->show_status(Color::Name::WHITE, "Name '" + name + "' is available!");
		}
	}

	void CashShopNameChangePossibleHandler::handle(InPacket& recv) const
	{
		recv.read_int(); // 0
		int8_t error = recv.read_byte();
		recv.read_int(); // 0

		if (error == 0)
		{
			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::WHITE, "Name change is available.");
		}
		else
		{
			std::string msg;

			switch (error)
			{
			case 1: msg = "A name change has already been submitted."; break;
			case 2: msg = "Must wait at least one month between name changes."; break;
			case 3: msg = "Cannot change name due to a recent ban."; break;
			default: msg = "Name change is not available."; break;
			}

			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::RED, msg);
		}
	}

	void CashShopTransferWorldHandler::handle(InPacket& recv) const
	{
		recv.read_int(); // 0
		int8_t error = recv.read_byte();
		recv.read_int(); // 0
		bool ok = recv.read_bool();

		if (ok)
		{
			int32_t world_count = recv.read_int();
			std::string world_list = "[CashShop] Available worlds: ";

			for (int32_t i = 0; i < world_count; i++)
			{
				std::string world_name = recv.read_string();

				if (i > 0)
					world_list += ", ";

				world_list += world_name;
			}

			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::YELLOW, world_list);
		}
		else if (error != 0)
		{
			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::RED, "World transfer failed (error=" + std::to_string(error) + ").");
		}
	}

	void CashGachaponResultHandler::handle(InPacket& recv) const
	{
		int8_t op = recv.read_byte();

		if (op == (int8_t)0xE4) // Open failed
		{
			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::RED, "Gachapon failed to open.");
		}
		else if (op == (int8_t)0xE5) // Open success
		{
			recv.read_long(); // box cash id
			int32_t remaining = recv.read_int();

			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::YELLOW, "[Gachapon] Opened! Remaining: " + std::to_string(remaining));
			// CashItemInfo + reward data follows
		}
	}
}