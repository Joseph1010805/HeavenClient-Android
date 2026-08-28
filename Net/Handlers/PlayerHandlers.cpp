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
#include "../../Console.h"
#include "../../IO/KeyAction.h"

#include "../Audio/Audio.h"
#include "../Character/Buff.h"
#include "../Character/CharEffect.h"
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

			// The tombstone and its sound. Both have been sitting in the data
			// and in Audio's sound table all along - Sound::Name::TOMBSTONE is
			// loaded at startup and was never once played, because nothing
			// implemented dying for it to belong to.
			player.show_effect_id(CharEffect::Id::TOMBSTONE);
			Sound(Sound::Name::TOMBSTONE).play();

			// The revive request, whichever frame asks for it. The tombstone
			// frame is the right one for dying; the plain message box is the
			// fallback if this UI version has no tombstone to draw.
			auto revive = [](bool)
				{
					// The map id must not be -1. Cosmic wraps its whole revive
					// path in "if (targetMapId != -1)", so -1 - the obvious
					// way to say "server, you pick" - makes it drop the
					// request silently. The value is otherwise unused when the
					// player is dead: the server always returns them to
					// getReturnMapId(), so the current map is a safe thing to
					// send.
					ChangeMapPacket(true, Stage::get().get_mapid(), "", false).dispatch();
				};

			if (UIDeathNotice::available())
				UI::get().emplace<UIDeathNotice>(revive);
			else
				UI::get().emplace<UIOk>("You have died. You will return to the nearest town.", revive);
		}
	}

	void KeymapHandler::handle(InPacket& recv) const
	{
		recv.skip(1);

		bool bound[90] = { false };

		for (uint8_t i = 0; i < 90; i++)
		{
			uint8_t type = recv.read_byte();
			int32_t action = recv.read_int();

			bound[i] = type != 0;

			UI::get().add_keymapping(i, type, action);
		}

		// A NEW CHARACTER ARRIVES WITH NOTHING BOUND.
		//
		// Cosmic writes no keymap when a character is created - the table is
		// simply empty - so all 90 entries come back as type 0 and the game
		// has no attack, no jump, no pickup and no menus. The arrows still
		// walk, because those are set in Keyboard's constructor, which is why
		// it looks like *some* input works and the rest is broken.
		//
		// So the client supplies the defaults. Done here rather than on the
		// server because it also repairs every character already created
		// empty, and because these are what the key bindings window will show
		// - the player sees them, and can change them, from the first minute.
		{
			struct Bind { uint8_t key; uint8_t type; int32_t action; };

			// Keys are MapleStory's own indices. 30/31/32 are A/S/D, which is
			// where a hand already rests, and 34 is G for sitting. The four
			// face buttons on a pad map to A, S, D and G through
			// Setting<Joystick_*>, so a controller gets attack, pickup, jump
			// and sit without anybody configuring anything.
			static const Bind defaults[] = {
				{ 30, 5, KeyAction::Id::ATTACK },   // A
				{ 31, 5, KeyAction::Id::PICKUP },   // S
				{ 32, 5, KeyAction::Id::JUMP },     // D
				{ 34, 5, KeyAction::Id::SIT },      // G

				// The menus, on the letters MapleStory has always used.
				{ 23, 4, KeyAction::Id::ITEMS },
				{ 25, 4, KeyAction::Id::STATS },
				{ 37, 4, KeyAction::Id::SKILLS },
				{ 38, 4, KeyAction::Id::QUESTLOG },
				{ 50, 4, KeyAction::Id::MINIMAP },
				{ 33, 4, KeyAction::Id::EQUIPMENT },
			};

			// Per KEY, not all-or-nothing.
			//
			// The first version only filled these in when the entire keymap
			// was empty, which sounds right and is not: a character that has
			// been played at all has SOMETHING bound - the client writes the
			// keymap back whenever a binding changes - and one stray entry was
			// enough to skip the lot. Josephgrey arrived with a partial keymap
			// and no attack key, and the defaults never ran.
			//
			// Filling each unbound key leaves anything the player has chosen
			// exactly as it is, and only supplies what is genuinely missing.
			int filled = 0;

			for (const Bind& b : defaults)
			{
				if (b.key < 90 && !bound[b.key])
				{
					UI::get().add_keymapping(b.key, b.type, b.action);
					filled++;
				}
			}

			if (filled > 0)
				printf("[*] keymap: filled in %d unbound key(s)\n", filled);
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