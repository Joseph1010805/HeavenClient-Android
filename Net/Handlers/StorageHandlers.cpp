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
#include "StorageHandlers.h"

#include "Helpers/ItemParser.h"

#include "../../IO/UI.h"
#include "../../IO/SecondScreen.h"
#include "../../IO/UITypes/UIStorage.h"

#include "../../Util/Silent.h"

namespace ms
{
	namespace
	{
		// The same arrangement the trade window uses: the lower panel where
		// there is one, an ordinary window over the game where there is not.
		UIStorage* bank_window(bool turn_to_it)
		{
			if (SecondScreen::available())
			{
				UIElement* page = turn_to_it
					? SecondScreen::open_storage()
					: SecondScreen::hosted(UIElement::Type::STORAGE);

				return static_cast<UIStorage*>(page);
			}

			if (auto existing = UI::get().get_element<UIStorage>())
				return existing.get();

			if (!turn_to_it)
				return nullptr;

			UI::get().emplace<UIStorage>();

			auto made = UI::get().get_element<UIStorage>();

			return made ? made.get() : nullptr;
		}

		// EVERY ITEM IN THE BANK, IN ORDER.
		//
		// Storage arrives as one flat list with no positions on it. That is
		// not an oversight: the server works an item's tab out from its id
		// and expects an index WITHIN that tab back, so the order here is
		// the numbering, and it must not be sorted or filtered on the way in.
		std::vector<UIStorage::Held> read_items(InPacket& recv, uint8_t count)
		{
			std::vector<UIStorage::Held> out;

			for (uint8_t i = 0; i < count; i++)
			{
				ItemParser::Skimmed one = ItemParser::skim_item(recv);

				out.push_back({ one.id, one.count });
			}

			return out;
		}

		// Which tab a storage packet is about.
		//
		// Cosmic writes InventoryType.getBitfieldEncoding(), which is 1 << (t
		// - 1) - so EQUIP is 1, USE 2, SETUP 4, ETC 8. Reading it as the type
		// number itself gets USE right by luck and everything else wrong.
		InventoryType::Id type_from_bits(int16_t bits)
		{
			for (int8_t t = 1; t < InventoryType::Id::LENGTH; t++)
				if (bits == (1 << (t - 1)))
					return static_cast<InventoryType::Id>(t);

			return InventoryType::Id::NONE;
		}
	}

	void StorageHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();

		switch (mode)
		{
		case 0x16:
		{
			// Opened by an NPC.
			int32_t npc = recv.read_int();
			int8_t slots = recv.read_byte();

			recv.read_short();          // 0x7E, the tabs the window may show
			recv.read_short();
			recv.read_int();

			int32_t meso = recv.read_int();

			recv.read_short();

			uint8_t count = static_cast<uint8_t>(recv.read_byte());

			std::vector<UIStorage::Held> items = read_items(recv, count);

			if (UIStorage* bank = bank_window(true))
				bank->opened(npc, slots, meso, std::move(items));

			break;
		}
		case 0x0D:
		case 0x09:
		{
			// Something went in (0x0D) or came out (0x09). Either way the
			// server sends the WHOLE tab it changed, not the item that moved.
			recv.read_byte();           // slots

			InventoryType::Id type = type_from_bits(recv.read_short());

			recv.read_short();
			recv.read_int();

			uint8_t count = static_cast<uint8_t>(recv.read_byte());

			std::vector<UIStorage::Held> items = read_items(recv, count);

			if (UIStorage* bank = bank_window(false))
				bank->replace_tab(type, std::move(items));

			break;
		}
		case 0x13:
		{
			// Money moved.
			recv.read_byte();           // slots
			recv.read_short();
			recv.read_short();
			recv.read_int();

			int32_t meso = recv.read_int();

			if (UIStorage* bank = bank_window(false))
				bank->set_meso(meso);

			break;
		}
		case 0x0A:
		case 0x0B:
		case 0x0C:
		{
			// Refused, and the reason is the mode byte itself.
			if (UIStorage* bank = bank_window(false))
				bank->refused(mode);

			break;
		}
		case 0x0F:
		{
			// Tidied. Cosmic only sends this with USE_STORAGE_ITEM_SORT on,
			// and it renumbers everything - so the whole list is replaced
			// rather than one tab.
			recv.read_byte();           // slots
			recv.read_byte();           // 124
			recv.skip(10);

			uint8_t count = static_cast<uint8_t>(recv.read_byte());

			std::vector<UIStorage::Held> items = read_items(recv, count);

			if (UIStorage* bank = bank_window(false))
				bank->replace_all(std::move(items));

			break;
		}
		default:
			Silent::report("StorageHandler",
				"unhandled mode " + std::to_string(static_cast<int32_t>(mode)));

			break;
		}
	}
}
