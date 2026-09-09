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

#include "../PacketHandler.h"

#include "../Character/Buff.h"
#include "../Character/Maplestat.h"

namespace ms
{
	// Parses keymappings and sends them to the Keyboard.
	class KeymapHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Parses skill macros.
	class SkillMacrosHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Notifies the client of changes in character stats.
	// Opcode: CHANGE_STATS(31)
	class ChangeStatsHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;

	private:
		bool handle_stat(Maplestat::Id stat, InPacket& recv) const;
		bool need_statsinfo_update(Maplestat::Id stat) const;
		bool need_skillbook_update(Maplestat::Id stat) const;
	};

	// Base class for packets which need to parse buffstats.
	class BuffHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;

	protected:
		virtual void handle_buff(InPacket& recv, Buffstat::Id stat) const = 0;
	};

	// Notifies the client that a buff was applied to the player.
	// Opcode: GIVE_BUFF(32)
	class ApplyBuffHandler : public BuffHandler
	{
	protected:
		void handle_buff(InPacket& recv, Buffstat::Id stat) const override;
	};

	// Notifies the client that a buff was cancelled.
	// Opcode: CANCEL_BUFF(33)
	class CancelBuffHandler : public BuffHandler
	{
	protected:
		void handle_buff(InPacket& recv, Buffstat::Id stat) const override;
	};

	// Force a stats recalculation.
	// Opcode: RECALCULATE_STATS(35)
	class RecalculateStatsHandler : public PacketHandler
	{
		void handle(InPacket&) const override;
	};

	// Updates the player's skills with the client.
	// Opcode: UPDATE_SKILL(36)
	class UpdateSkillHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Notifies the client that a skill is on cooldown.
	// Opcode: ADD_COOLDOWN(234)
	class AddCooldownHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// A NEW MONSTER CARD, as it drops.
	//
	// The book was write-only: login filled it in and nothing ever added to
	// it afterwards, so a card picked up mid-session did not appear until the
	// next login - and there was no page to see it on either way.
	//
	// Opcode: MONSTER_BOOK_ADD_CARD(83)
	class MonsterBookAddCardHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Which card is on the cover.
	// Opcode: MONSTER_BOOK_SET_COVER(84)
	class MonsterBookCoverHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};
}
