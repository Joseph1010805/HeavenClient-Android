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
//
// The party layouts here were worked out against Cosmic by the OpenStory
// fork (https://github.com/rdiol12/OpenStory), AGPL-3.0, and are used with
// thanks. Two of them differ from what the v83 documentation implies - see
// the comments on parse_party_data and on case 0x0C.
//
#include "SocialHandlers.h"

#include "../Packets/GameplayPackets.h"

#include "../../Configuration.h"
#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UINotice.h"
#include "../../IO/UITypes/UIStatusMessenger.h"

namespace ms
{
	namespace
	{
		void parse_party_data(InPacket& recv, int32_t partyid)
		{
			// The v83 full party block, always six slots whether or not they
			// are filled:
			//   6x int cid, 6x padded(13) name, 6x int job, 6x int level,
			//   6x int channel, int leader_cid, 6x int mapid,
			// then door data we do not use.
			//
			// Note the leader id sits BETWEEN the channels and the map ids.
			// Reading it after the map ids - which is what the layout looks
			// like at first glance - shifts every map id by one slot.
			if (recv.length() < 4 * 6)
				return;

			int32_t cids[6];
			for (int i = 0; i < 6; i++)
				cids[i] = recv.read_int();

			std::string names[6];
			for (int i = 0; i < 6; i++)
				names[i] = recv.read_padded_string(13);

			int32_t jobs[6];
			for (int i = 0; i < 6; i++)
				jobs[i] = recv.read_int();

			int32_t levels[6];
			for (int i = 0; i < 6; i++)
				levels[i] = recv.read_int();

			int32_t channels[6];
			for (int i = 0; i < 6; i++)
				channels[i] = recv.read_int();

			int32_t leader_cid = recv.read_int();

			int32_t mapids[6];
			for (int i = 0; i < 6; i++)
				mapids[i] = recv.read_int();

			std::vector<PartyMember> members;

			for (int i = 0; i < 6; i++)
			{
				if (cids[i] == 0)
					continue;

				PartyMember member;
				member.cid = cids[i];
				member.name = names[i];
				member.job = static_cast<int16_t>(jobs[i]);
				member.level = static_cast<int16_t>(levels[i]);
				member.channel = channels[i];
				member.mapid = mapids[i];
				member.online = channels[i] >= 0;
				members.push_back(member);
			}

			Stage::get().get_player().get_party().update(partyid, members, leader_cid);
		}
	}

	void BuddyListHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();

		// 7 is "here is the whole list", which is what arrives at login and
		// after every add or removal. The other modes are notifications the
		// list refresh follows anyway.
		if (mode != 7)
			return;

		int8_t count = recv.read_byte();

		std::vector<BuddyEntry> entries;

		for (int8_t i = 0; i < count; i++)
		{
			BuddyEntry e;

			e.cid = recv.read_int();

			// Fixed 13 bytes, null padded - NOT a length-prefixed string.
			// Reading it as one would swallow the rest of the entry.
			e.name = recv.read_padded_string(13);

			recv.read_byte();				// opposite status
			e.channel = recv.read_int();	// channel - 1, so < 0 is offline
			e.group = recv.read_padded_string(13);

			recv.read_int();				// map id, always zero here

			entries.push_back(std::move(e));
		}

		// A trailing int per entry, which Cosmic writes as zeroes.
		for (int8_t i = 0; i < count; i++)
			recv.read_int();

		Stage::get().get_player().get_buddies().update(std::move(entries));
	}

	void UpdatePartyMemberHpHandler::handle(InPacket& recv) const
	{
		// int cid, int hp, int maxhp - PacketCreator.updatePartyMemberHP.
		if (recv.length() < 12)
			return;

		int32_t cid = recv.read_int();
		int32_t hp = recv.read_int();
		int32_t maxhp = recv.read_int();

		Stage::get().get_player().get_party().update_member_hp(cid, hp, maxhp);
	}

	void PartyOperationHandler::handle(InPacket& recv) const
	{
		if (!recv.available())
			return;

		int8_t operation = recv.read_byte();
		auto messenger = UI::get().get_element<UIStatusMessenger>();

		switch (operation)
		{
		case 4:
		{
			// An invitation arrived.
			if (!Setting<AllowPartyInvite>::get().load())
				break;

			if (recv.length() < 4)
				break;

			int32_t partyid = recv.read_int();
			std::string from_name = recv.read_string();

			// Party-search invites arrive prefixed. The server matches the
			// refusal back by the name as sent, so strip the prefix only for
			// what we show, never for what we send back.
			std::string display_name = from_name;

			if (display_name.substr(0, 4) == "PS: ")
				display_name = display_name.substr(4);

			UI::get().emplace<UIYesNo>(
				display_name + " has invited you to their party.",
				[partyid, from_name](bool yes)
				{
					if (yes)
						JoinPartyPacket(partyid).dispatch();
					else
						DenyPartyInvitePacket(from_name).dispatch();
				}
			);

			break;
		}
		case 7:
		{
			// A silent update - somebody changed map, channel, or logged on
			// or off. No message, just the new roster.
			if (recv.length() < 4)
				break;

			int32_t partyid = recv.read_int();
			parse_party_data(recv, partyid);
			break;
		}
		case 8:
		{
			// The party now exists.
			if (recv.length() < 4)
				break;

			recv.read_int();	// party id
			recv.read_int();	// door: town map
			recv.read_int();	// door: area map
			recv.read_int();	// door: x
			recv.read_int();	// door: y

			if (messenger)
				messenger->show_status(Color::Name::WHITE, "You have created a party.");

			break;
		}
		case 0x0C:
		{
			// Leave, expel or disband, which share an opcode and are told
			// apart by a status byte:
			//   int  party_id
			//   int  target_cid
			//   byte status			(0 = disband, 1 = leave or expel)
			//   status 0: int party_id again
			//   status 1: byte expelled, string target_name, <party data>
			//
			// Reading that status byte as two separate booleans - which is
			// the obvious way to write it - walks the cursor past the end of
			// the disband form and crashes the client.
			if (recv.length() < 4)
				break;

			int32_t partyid = recv.read_int();
			int32_t target_cid = recv.read_int();
			int8_t status = recv.read_byte();

			int32_t my_cid = Stage::get().get_player().get_oid();

			if (status == 0)
			{
				if (recv.length() >= 4)
					recv.read_int();	// party id repeated

				Stage::get().get_player().get_party().clear();

				if (messenger)
					messenger->show_status(Color::Name::WHITE, "The party has been disbanded.");
			}
			else
			{
				bool expelled = recv.read_byte() != 0;
				std::string target_name = recv.read_string();

				if (target_cid == my_cid)
				{
					Stage::get().get_player().get_party().clear();

					if (messenger)
						messenger->show_status(
							expelled ? Color::Name::RED : Color::Name::WHITE,
							expelled
								? std::string("You have been expelled from the party.")
								: std::string("You have left the party.")
						);
				}
				else
				{
					if (recv.length() >= 4 * 6)
						parse_party_data(recv, partyid);

					if (messenger)
						messenger->show_status(
							Color::Name::WHITE,
							expelled
								? target_name + " has been expelled."
								: target_name + " has left the party."
						);
				}
			}

			break;
		}
		case 0x0F:
		{
			// Somebody joined.
			if (recv.length() < 4)
				break;

			int32_t partyid = recv.read_int();
			std::string new_member = recv.read_string();

			if (recv.length() >= 4 * 6)
				parse_party_data(recv, partyid);

			if (messenger)
				messenger->show_status(Color::Name::WHITE, new_member + " has joined the party.");

			break;
		}
		case 0x1B:
		{
			// The leader changed.
			if (recv.length() < 4)
				break;

			recv.read_int();	// new leader cid
			recv.read_byte();	// always 0

			if (messenger)
				messenger->show_status(Color::Name::WHITE, "The party leader has changed.");

			break;
		}
		case 0x23:
		{
			// A party door moved. Doors are not built yet.
			if (recv.available())
				recv.read_short();

			break;
		}

		// The rest are refusals. Note 12 is missing on purpose - that is
		// 0x0C above, which is a real message rather than an error.
		case 10:
			if (messenger)
				messenger->show_status(Color::Name::RED, "A beginner can't create a party.");

			break;
		case 13:
			if (messenger)
				messenger->show_status(Color::Name::RED, "You are not in a party.");

			break;
		case 16:
		{
			// The server does not say who, so fall back on who we last asked.
			const std::string& target = InviteToPartyPacket::last_invited_name();
			std::string who = target.empty() ? "That player" : target;

			if (messenger)
				messenger->show_status(Color::Name::RED, who + " is already in a party.");

			break;
		}
		case 17:
			if (messenger)
				messenger->show_status(Color::Name::RED, "The party is full.");

			break;
		case 19:
			if (messenger)
				messenger->show_status(Color::Name::RED, "Unable to find the player in this channel.");

			break;
		case 21:
			if (messenger)
				messenger->show_status(Color::Name::RED, "This player is blocking party invitations.");

			break;
		case 22:
		{
			std::string name = recv.available() ? recv.read_string() : "";

			if (messenger)
				messenger->show_status(Color::Name::RED, name + " is already handling another invitation.");

			break;
		}
		case 23:
		{
			std::string name = recv.available() ? recv.read_string() : "";

			if (messenger)
				messenger->show_status(Color::Name::WHITE, name + " has denied the invitation.");

			break;
		}
		case 25:
			if (messenger)
				messenger->show_status(Color::Name::RED, "You cannot kick in this map.");

			break;
		case 28:
		case 29:
			if (messenger)
				messenger->show_status(Color::Name::RED, "Leader change only to a nearby party member.");

			break;
		case 30:
			if (messenger)
				messenger->show_status(Color::Name::RED, "Leader change only on the same channel.");

			break;
		default:
			break;
		}
	}
}
