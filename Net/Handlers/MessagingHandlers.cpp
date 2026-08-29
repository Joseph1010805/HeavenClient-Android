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
#include "MessagingHandlers.h"

#include "../Data/QuestData.h"

#include "../Data/ItemData.h"
#include "../Gameplay/Stage.h"
#include "../IO/UI.h"

#include "../IO/UITypes/UIStatusMessenger.h"
#include "../IO/UITypes/UIChatbar.h"

namespace ms
{
	// Modes:
	// 0 - Item(0) or Meso(1) 
	// 3 - Exp gain
	// 4 - Fame
	// 5 - Mesos
	// 6 - Guild points
	void ShowStatusInfoHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();

		if (mode == 0)
		{
			int8_t mode2 = recv.read_byte();

			if (mode2 == -1)
			{
				show_status(Color::Name::WHITE, "You can't get anymore items.");
			}
			else if (mode2 == 0)
			{
				int32_t itemid = recv.read_int();
				int32_t qty = recv.read_int();

				const ItemData& idata = ItemData::get(itemid);

				if (!idata.is_valid())
					return;

				std::string name = idata.get_name();

				if (name.length() > 21)
				{
					name.substr(0, 21);
					name += "..";
				}

				InventoryType::Id type = InventoryType::by_item_id(itemid);

				std::string tab = "";

				switch (type)
				{
				case InventoryType::Id::EQUIP:
					tab = "Eqp";
					break;
				case InventoryType::Id::USE:
					tab = "Use";
					break;
				case InventoryType::Id::SETUP:
					tab = "Setup";
					break;
				case InventoryType::Id::ETC:
					tab = "Etc";
					break;
				case InventoryType::Id::CASH:
					tab = "Cash";
					break;
				default:
					tab = "UNKNOWN";
					break;
				}

				// TODO: show_status(Color::Name::WHITE, "You have lost items in the " + tab + " tab (" + name + " " + std::to_string(qty) + ")");

				if (qty < 0)
					show_status(Color::Name::WHITE, "You have lost an item in the " + tab + " tab (" + name + ")");
				else if (qty == 1)
					show_status(Color::Name::WHITE, "You have gained an item in the " + tab + " tab (" + name + ")");
				else
					show_status(Color::Name::WHITE, "You have gained items in the " + tab + " tab (" + name + " " + std::to_string(qty) + ")");
			}
			else if (mode2 == 1)
			{
				recv.skip(1);

				int32_t gain = recv.read_int();
				std::string sign = (gain < 0) ? "-" : "+";

				show_status(Color::Name::WHITE, "You have gained mesos (" + sign + std::to_string(gain) + ")");
			}
			else
			{
				show_status(Color::Name::RED, "Mode: 0, Mode 2: " + std::to_string(mode2) + " is not handled.");
			}
		}
		else if (mode == 1)
		{
			// A quest changed. This is the ONLY thing that tells the client a
			// quest was taken or handed in, and there was no case for it -
			// every quest update fell through to the bottom and was shown to
			// the player as "Mode: 1 is not handled" in red.
			//
			//   short questid
			//   byte  status   0 given up, 1 under way, 2 finished
			//   then, by status: nothing / the progress string / the time
			int16_t questid = recv.read_short();
			int8_t status = recv.read_byte();

			Questlog& log = Stage::get().get_player().get_quests();
			const QuestData& data = QuestData::get(questid);

			std::string name = data.is_valid()
				? data.get_name()
				: ("Quest " + std::to_string(questid));

			switch (status)
			{
			case 0:
				log.forget(questid);
				show_status(Color::Name::WHITE, "Quest given up - " + name);
				break;
			case 1:
			{
				// The progress string. For a kill count it is three digits
				// per monster, in the order the quest lists them, and it
				// arrives again after every kill.
				bool was_on_it = log.is_started(questid);

				log.set_started(questid, recv.read_string());

				if (!was_on_it)
					show_status(Color::Name::YELLOW, "Quest started - " + name);

				break;
			}
			case 2:
				log.set_completed(questid, recv.read_long());
				show_status(Color::Name::YELLOW, "Quest complete - " + name);
				break;
			default:
				break;
			}
		}
		else if (mode == 3)
		{
			bool white = recv.read_bool();
			int32_t gain = recv.read_int();
			bool inchat = recv.read_bool();
			int32_t bonus1 = recv.read_int();

			recv.read_short();
			recv.read_int();	// bonus 2

			// THE EXTRA BYTE THAT ONLY EXISTS WHEN inchat IS SET.
			//
			// Cosmic writes it - `if (inChat) p.writeByte(0);` in
			// getShowExpGain - and this read a fixed layout, so every field
			// after it was one byte out. Harmless only because nothing after
			// it was used for anything.
			if (inchat)
				recv.read_byte();

			recv.read_bool();	// 'event or party'
			recv.read_int();	// bonus 3
			recv.read_int();	// bonus 4 - equip
			recv.read_int();	// bonus 5 - internet cafe
			recv.read_int();	// rainbow week - was left unread

			std::string message = "You have gained experience (+" + std::to_string(gain) + ")";

			// inchat is EXPERIENCE FROM A QUEST, and it was the one kind the
			// player never saw.
			//
			// It said "Mode: 3, inchat is not handled" in red, which is a
			// message about the client rather than about the game - so
			// finishing a quest, or answering one of Robin's tutorial
			// questions, silently levelled you up with nothing to say why.
			//
			// The flag means the server wants it in the CHAT LOG rather than
			// floating over the character, so that is where it goes when there
			// is a chat log to put it in.
			if (inchat)
			{
				if (auto chatbar = UI::get().get_element<UIChatbar>())
					chatbar->send_chatline(message, UIChatbar::LineType::YELLOW);
				else
					show_status(Color::Name::YELLOW, message);

				if (bonus1 > 0)
					show_status(Color::Name::YELLOW, "+ Bonus EXP (+" + std::to_string(bonus1) + ")");
			}
			else
			{
				show_status(white ? Color::Name::WHITE : Color::Name::YELLOW, message);

				if (bonus1 > 0)
					show_status(Color::Name::YELLOW, "+ Bonus EXP (+" + std::to_string(bonus1) + ")");
			}
		}
		else if (mode == 4)
		{
			int32_t gain = recv.read_int();
			std::string sign = (gain < 0) ? "-" : "+";

			// TODO: Lose fame?
			show_status(Color::Name::WHITE, "You have gained fame. (" + sign + std::to_string(gain) + ")");
		}
		else
		{
			show_status(Color::Name::RED, "Mode: " + std::to_string(mode) + " is not handled.");
		}
	}

	void ShowStatusInfoHandler::show_status(Color::Name color, const std::string& message) const
	{
		if (auto messenger = UI::get().get_element<UIStatusMessenger>())
			messenger->show_status(color, message);
	}

	void ServerMessageHandler::handle(InPacket& recv) const
	{
		int8_t type = recv.read_byte();
		bool servermessage = recv.inspect_bool();

		if (servermessage)
			recv.skip(1);

		std::string message = recv.read_string();

		// EVERY SERVER MESSAGE WAS READ AND THROWN AWAY.
		//
		// Only type 4, the scrolling banner, was ever shown. The rest were
		// parsed off the wire correctly and then dropped on the floor - so a
		// megaphone reached every client in the channel and none of them
		// displayed a word of it. The shout worked; the hearing did not.
		//
		// That covers more than megaphones: notices, GM announcements and the
		// coloured server lines all arrive this way.
		//
		//   0  notice        1  popup           2  megaphone
		//   3  super mega    4  scrolling       5  pink
		//   6  light blue    7  NPC-styled
		//
		// The extra fields still have to be read even when they are unused -
		// leaving them on the wire is how the NEXT packet gets misread.
		Color::Name colour = Color::Name::WHITE;

		if (type == 3)
		{
			recv.read_byte(); // channel
			recv.read_bool(); // whether to show the megaphone ear

			colour = Color::Name::YELLOW;
		}
		else if (type == 4)
		{
			UI::get().set_scrollnotice(message);
			return;
		}
		else if (type == 7)
		{
			recv.read_int(); // npcid
		}
		else if (type == 2)
		{
			colour = Color::Name::YELLOW;
		}
		else if (type == 5)
		{
			colour = Color::Name::RED;
		}

		if (message.empty())
			return;

		// The chat log, because these are things somebody SAID, and chat is
		// where you look for those - and because it stays readable after the
		// moment has passed, which a floating message does not.
		if (auto chatbar = UI::get().get_element<UIChatbar>())
		{
			UIChatbar::LineType line = UIChatbar::LineType::WHITE;

			if (colour == Color::Name::YELLOW)
				line = UIChatbar::LineType::YELLOW;
			else if (colour == Color::Name::RED)
				line = UIChatbar::LineType::RED;

			chatbar->send_chatline(message, line);
		}
		else if (auto messenger = UI::get().get_element<UIStatusMessenger>())
		{
			messenger->show_status(colour, message);
		}
	}

	void WeekEventMessageHandler::handle(InPacket& recv) const
	{
		recv.read_byte(); // TODO: Always 0xFF, Check this!

		std::string message = recv.read_string();

		static const std::string MAPLETIP = "[MapleTip]";

		if (message.substr(0, MAPLETIP.length()).compare("[MapleTip]"))
			message = "[Notice] " + message;

		UI::get().get_element<UIChatbar>()->send_chatline(message, UIChatbar::LineType::YELLOW);
	}

	void ChatReceivedHandler::handle(InPacket& recv) const
	{
		int32_t charid = recv.read_int();

		recv.read_bool(); // 'gm'

		std::string message = recv.read_string();
		int8_t type = recv.read_byte();

		if (auto character = Stage::get().get_character(charid))
		{
			message = character->get_name() + ": " + message;
			character->speak(message);
		}

		auto linetype = static_cast<UIChatbar::LineType>(type);

		if (auto chatbar = UI::get().get_element<UIChatbar>())
			chatbar->send_chatline(message, linetype);
	}

	void ScrollResultHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();
		bool success = recv.read_bool();
		bool destroyed = recv.read_bool();

		recv.read_short(); // Legendary spirit if 1

		CharEffect::Id effect;
		Messages::Type message;

		if (success)
		{
			effect = CharEffect::Id::SCROLL_SUCCESS;
			message = Messages::Type::SCROLL_SUCCESS;
		}
		else
		{
			effect = CharEffect::Id::SCROLL_FAILURE;

			if (destroyed)
				message = Messages::Type::SCROLL_DESTROYED;
			else
				message = Messages::Type::SCROLL_FAILURE;
		}

		Stage::get().show_character_effect(cid, effect);

		if (Stage::get().is_player(cid))
		{
			if (auto chatbar = UI::get().get_element<UIChatbar>())
				chatbar->display_message(message, UIChatbar::LineType::RED);

			UI::get().enable();
		}
	}

	void ShowItemGainInChatHandler::handle(InPacket& recv) const
	{
		int8_t mode1 = recv.read_byte();

		if (mode1 == 3)
		{
			int8_t mode2 = recv.read_byte();

			if (mode2 == 1) // This is actually 'item gain in chat'
			{
				int32_t itemid = recv.read_int();
				int32_t qty = recv.read_int();

				const ItemData& idata = ItemData::get(itemid);

				if (!idata.is_valid())
					return;

				std::string name = idata.get_name();
				std::string sign = (qty < 0) ? "-" : "+";
				std::string message = "Gained an item: " + name + " (" + sign + std::to_string(qty) + ")";

				if (auto chatbar = UI::get().get_element<UIChatbar>())
					chatbar->send_chatline(message, UIChatbar::LineType::BLUE);
			}
		}
		else if (mode1 == 13) // card effect
		{
			Stage::get().get_player().show_effect_id(CharEffect::Id::MONSTER_CARD);
		}
		else if (mode1 == 18) // intro effect
		{
			recv.read_string(); // path
		}
		else if (mode1 == 23) // info
		{
			recv.read_string();	// path
			recv.read_int();	// some int
		}
		else // Buff effect
		{
			int32_t skillid = recv.read_int();

			// More bytes, but we don't need them
			Stage::get().get_combat().show_player_buff(skillid);
		}
	}
}