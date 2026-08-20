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
#include "PlayerHandlers.h"

#include "../Character/Buff.h"
#include "../Gameplay/Stage.h"
#include "../IO/UI.h"
#include "../IO/UITypes/UIBuffList.h"
#include "../IO/UITypes/UIStatsinfo.h"
#include "../IO/UITypes/UISkillbook.h"
#include "../IO/UITypes/UINotice.h"
#include "../Net/Packets/GameplayPackets.h"

namespace ms
{
	namespace
	{
		// Whether the player has already been told they are dead, so the
		// prompt is raised once rather than on every stat packet that arrives
		// while HP sits at zero.
		bool death_prompted = false;

		// The client had no notion of dying. Char::State::DIED exists and is
		// read in two places, but nothing ever set it, so reaching 0 HP left
		// the character standing about as though nothing had happened.
		//
		// The server has already done its half by this point - Cosmic's
		// playerDead() cancels buffs and applies the penalty - and is waiting
		// for a revive request that never came. That request is just a change
		// map with its first byte set, which ChangeMapPacket already supports;
		// only the detection and the prompt were missing.
		void check_death()
		{
			Player& player = Stage::get().get_player();

			if (player.get_stats().get_stat(Maplestat::Id::HP) > 0)
			{
				death_prompted = false;
				return;
			}

			if (death_prompted)
				return;

			death_prompted = true;

			// Maps to the DEAD stance through Stance::by_state, and stops the
			// character being walked around while dead.
			player.set_state(Char::State::DIED);

			UI::get().emplace<UIOk>(
				"You have died. You will return to the nearest town.",
				[](bool)
				{
					// -1 asks the server to choose the return map itself.
					ChangeMapPacket(true, -1, "", false).dispatch();
				});
		}
	}

	void KeymapHandler::handle(InPacket& recv) const
	{
		recv.skip(1);

		for (uint8_t i = 0; i < 90; i++)
		{
			uint8_t type = recv.read_byte();
			int32_t action = recv.read_int();

			UI::get().add_keymapping(i, type, action);
		}
	}


	void SkillMacrosHandler::handle(InPacket& recv) const
	{
		uint8_t size = recv.read_byte();

		for (uint8_t i = 0; i < size; i++)
		{
			recv.read_string(); // name
			recv.read_byte(); // 'shout' byte
			recv.read_int(); // skill 1
			recv.read_int(); // skill 2
			recv.read_int(); // skill 3
		}
	}


	void ChangeStatsHandler::handle(InPacket& recv) const
	{
		recv.read_bool(); // 'itemreaction'
		int32_t updatemask = recv.read_int();

		bool recalculate = false;

		for (auto iter : Maplestat::codes)
			if (updatemask & iter.second)
				recalculate |= handle_stat(iter.first, recv);

		if (recalculate)
			Stage::get().get_player().recalc_stats(false);

		check_death();

		UI::get().enable();
	}

	bool ChangeStatsHandler::handle_stat(Maplestat::Id stat, InPacket& recv) const
	{
		Player& player = Stage::get().get_player();

		bool recalculate = false;

		switch (stat)
		{
		case Maplestat::SKIN:
			player.change_look(stat, recv.read_short());
			break;
		case Maplestat::FACE:
		case Maplestat::HAIR:
			player.change_look(stat, recv.read_int());
			break;
		case Maplestat::LEVEL:
			player.change_level(recv.read_byte());
			break;
		case Maplestat::JOB:
			player.change_job(recv.read_short());
			break;
		case Maplestat::EXP:
			player.get_stats().set_exp(recv.read_int());
			break;
		case Maplestat::MESO:
			player.get_inventory().set_meso(recv.read_int());
			break;
		default:
			player.get_stats().set_stat(stat, recv.read_short());
			recalculate = true;
			break;
		}

		bool update_statsinfo = need_statsinfo_update(stat);

		if (update_statsinfo && !recalculate)
			if (auto statsinfo = UI::get().get_element<UIStatsinfo>())
				statsinfo->update_stat(stat);

		bool update_skillbook = need_skillbook_update(stat);

		if (update_skillbook)
		{
			int16_t value = player.get_stats().get_stat(stat);

			if (auto skillbook = UI::get().get_element<UISkillbook>())
				skillbook->update_stat(stat, value);
		}

		return recalculate;
	}

	bool ChangeStatsHandler::need_statsinfo_update(Maplestat::Id stat) const
	{
		switch (stat)
		{
		case Maplestat::JOB:
		case Maplestat::STR:
		case Maplestat::DEX:
		case Maplestat::INT:
		case Maplestat::LUK:
		case Maplestat::HP:
		case Maplestat::MAXHP:
		case Maplestat::MP:
		case Maplestat::MAXMP:
		case Maplestat::AP:
			return true;
		default:
			return false;
		}
	}

	bool ChangeStatsHandler::need_skillbook_update(Maplestat::Id stat) const
	{
		switch (stat)
		{
		case Maplestat::JOB:
		case Maplestat::SP:
			return true;
		default:
			return false;
		}
	}


	void BuffHandler::handle(InPacket& recv) const
	{
		uint64_t firstmask = recv.read_long();
		uint64_t secondmask = recv.read_long();

		switch (secondmask)
		{
		case Buffstat::BATTLESHIP:
			handle_buff(recv, Buffstat::BATTLESHIP);
			return;
		}

		for (auto& iter : Buffstat::first_codes)
			if (firstmask & iter.second)
				handle_buff(recv, iter.first);

		for (auto& iter : Buffstat::second_codes)
			if (secondmask & iter.second)
				handle_buff(recv, iter.first);

		Stage::get().get_player().recalc_stats(false);
	}

	void ApplyBuffHandler::handle_buff(InPacket& recv, Buffstat::Id bs) const
	{
		int16_t value = recv.read_short();
		int32_t skillid = recv.read_int();
		int32_t duration = recv.read_int();

		Stage::get().get_player().give_buff({ bs, value, skillid, duration });

		if (auto bufflist = UI::get().get_element<UIBuffList>())
			bufflist->add_buff(skillid, duration);
	}

	void CancelBuffHandler::handle_buff(InPacket&, Buffstat::Id bs) const
	{
		Stage::get().get_player().cancel_buff(bs);
	}


	void RecalculateStatsHandler::handle(InPacket&) const
	{
		Stage::get().get_player().recalc_stats(false);
	}

	void UpdateSkillHandler::handle(InPacket& recv) const
	{
		recv.skip(3);

		int32_t skillid = recv.read_int();
		int32_t level = recv.read_int();
		int32_t masterlevel = recv.read_int();
		int64_t expire = recv.read_long();

		Stage::get().get_player().change_skill(skillid, level, masterlevel, expire);

		if (auto skillbook = UI::get().get_element<UISkillbook>())
			skillbook->update_skills(skillid);

		UI::get().enable();
	}

	void AddCooldownHandler::handle(InPacket& recv) const
	{
		int32_t skill_id = recv.read_int();
		int16_t cooltime = recv.read_short();

		Stage::get().get_player().add_cooldown(skill_id, cooltime);
	}
}