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
#include "MapObjectHandlers.h"

#include "Helpers/LoginParser.h"
#include "Helpers/MovementParser.h"

#include "../Audio/Audio.h"
#include "../Gameplay/Stage.h"
#include "../Gameplay/Spawn.h"

namespace ms
{
	void SpawnCharHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();

		// We don't need to spawn the player twice
		if (Stage::get().is_player(cid))
			return;

		uint8_t level = recv.read_byte();
		std::string name = recv.read_string();

		recv.read_string();	// guildname
		recv.read_short();	// guildlogobg
		recv.read_byte();	// guildlogobgcolor
		recv.read_short();	// guildlogo
		recv.read_byte();	// guildlogocolor

		recv.skip(8);

		bool morphed = recv.read_int() == 2;
		int32_t buffmask1 = recv.read_int();
		int16_t buffvalue = 0;

		if (buffmask1 != 0)
			buffvalue = morphed ? recv.read_short() : recv.read_byte();

		recv.read_int(); // buffmask 2

		recv.skip(43);

		recv.read_int(); // 'mount'

		recv.skip(61);

		int16_t job = recv.read_short();
		LookEntry look = LoginParser::parse_look(recv);

		recv.read_int(); // count of 5110000 
		recv.read_int(); // 'itemeffect'
		recv.read_int(); // 'chair'

		Point<int16_t> position = recv.read_point();
		int8_t stance = recv.read_byte();

		recv.skip(3);

		for (size_t i = 0; i < 3; i++)
		{
			int8_t available = recv.read_byte();

			if (available == 1)
			{
				recv.read_byte();	// 'byte2'
				recv.read_int();	// petid
				recv.read_string();	// name
				recv.read_int();	// unique id
				recv.read_int();
				recv.read_point();	// pos
				recv.read_byte();	// stance
				recv.read_int();	// fhid
			}
			else
			{
				break;
			}
		}

		recv.read_int(); // mountlevel
		recv.read_int(); // mountexp
		recv.read_int(); // mounttiredness

		// TODO: Shop stuff
		recv.read_byte();
		// TODO: Shop stuff end

		bool chalkboard = recv.read_bool();
		std::string chalktext = chalkboard ? recv.read_string() : "";

		recv.skip(3);
		recv.read_byte(); // team

		Stage::get().get_chars().spawn(
			{ cid, look, level, job, name, stance, position }
		);
	}

	void RemoveCharHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();

		Stage::get().get_chars().remove(cid);
	}

	void SpawnPetHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();
		Optional<Char> character = Stage::get().get_character(cid);

		if (!character)
			return;

		uint8_t petindex = recv.read_byte();
		int8_t mode = recv.read_byte();

		if (mode == 1)
		{
			recv.skip(1);

			int32_t itemid = recv.read_int();
			std::string name = recv.read_string();
			int32_t uniqueid = recv.read_int();

			recv.skip(4);

			Point<int16_t> pos = recv.read_point();
			uint8_t stance = recv.read_byte();
			int32_t fhid = recv.read_int();

			character->add_pet(petindex, itemid, name, uniqueid, pos, stance, fhid);
		}
		else if (mode == 0)
		{
			bool hunger = recv.read_bool();

			character->remove_pet(petindex, hunger);
		}
	}

	void CharMovedHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();
		recv.skip(4);
		std::vector<Movement> movements = MovementParser::parse_movements(recv);

		Stage::get().get_chars().send_movement(cid, movements);
	}

	void UpdateCharLookHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();
		recv.read_byte();
		LookEntry look = LoginParser::parse_look(recv);

		Stage::get().get_chars().update_look(cid, look);
	}

	void ShowForeignEffectHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();
		int8_t effect = recv.read_byte();

		if (effect == 10) // recovery
		{
			recv.read_byte(); // 'amount'
		}
		else if (effect == 13) // card effect
		{
			Stage::get().show_character_effect(cid, CharEffect::MONSTER_CARD);
		}
		else if (recv.available()) // skill
		{
			int32_t skillid = recv.read_int();
			recv.read_byte(); // 'direction'
			// 9 more bytes after this

			Stage::get().get_combat().show_buff(cid, skillid, effect);
		}
		else
		{
			// TODO: Blank
		}
	}

	namespace
	{
		// Reads the tail of a mob spawn: the effect, and the marker saying
		// whether this is a fresh spawn.
		//
		// Three shapes arrive here, and the server gives no flag to say which:
		//
		//   no effect        the marker on its own, -1 or -2
		//   summoned, plain  -3, then the parent monster's object id
		//   with an effect   a pad byte, a zero short, then the marker -
		//                    or, if summoned, the parent's object id
		//
		// The last two both occupy exactly four bytes, so the read position
		// stays right whichever it is and only the meaning has to be settled.
		// Value decides it: the marker sits in the top byte with zeroes under
		// it, which an object id would have to exceed four billion to imitate.
		//
		// Getting this wrong is a boss problem specifically. Ordinary monsters
		// spawn without an effect and without a parent, so they take the first
		// shape and always worked; spawn animations and multi-part bosses are
		// what land in the other two.
		int8_t read_spawn_marker(InPacket& recv, int8_t effect)
		{
			// A summoned monster with no spawn effect.
			if (effect == -3)
			{
				int32_t parent = recv.read_int();

				printf("[*] spawn: summoned, parent oid %d\n", parent);

				// It appears when its parent dies rather than fading in.
				return -1;
			}

			if (effect <= 0)
				return effect; // the byte was the marker itself

			// Effect 15 pads an extra byte, but only when not summoned, which
			// is what the remaining length tells us: four bytes plus the team
			// byte and a trailing int is nine.
			if (effect == 15 && recv.length() > 9)
				recv.read_byte();

			int32_t tail = recv.read_int();
			int8_t marker = static_cast<int8_t>((tail >> 24) & 0xFF);

			if ((tail & 0x00FFFFFF) == 0 && (marker == -1 || marker == -2))
			{
				printf("[*] spawn: effect %d, marker %d\n", effect, marker);

				return marker;
			}

			printf("[*] spawn: summoned with effect %d, parent oid %d\n", effect, tail);

			return -1; // an object id, so summoned again
		}
	}

	void SpawnMobHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		recv.read_byte(); // 5 if controller == null
		int32_t id = recv.read_int();

		// The monster's temporary-status block. Cosmic writes 16 bytes here -
		// either four ints of status mask, or a plain 16 byte pad when no
		// controller is requested - so skipping 22 read the position, stance
		// and foothold six bytes past where they actually are. The mob was
		// then placed at a nonsense coordinate on a nonsense foothold and
		// never appeared, even though the spawn packets were arriving fine.
		recv.skip(16);

		Point<int16_t> position = recv.read_point();
		int8_t stance = recv.read_byte();

		recv.skip(2);

		uint16_t fh = recv.read_short();

		// Spawn effect, then a marker saying whether this is a new spawn.
		//
		// The server writes the effect block only when there is an effect, but
		// writes the marker either way - so with no effect the first byte here
		// IS the marker, and with one it follows the block. Reading straight
		// on to the team byte therefore worked by luck for ordinary monsters
		// and drifted a byte for anything spawning with an effect, which is
		// mostly bosses: the "King Slime spawn", "The Boss" and summoning
		// animations all take this path.
		int8_t effect = recv.read_byte();
		int8_t newspawn = read_spawn_marker(recv, effect);

		int8_t team = recv.read_byte();

		recv.skip(4);

		Stage::get().get_mobs().spawn(
			{ oid, id, 0, stance, fh, newspawn == -2, team, position }
		);
	}

	void MoveMobResponseHandler::handle(InPacket& recv) const
	{
		// PacketCreator.moveMonsterResponse:
		//   int   objectid
		//   short moveid
		//   bool  useSkills
		//   short currentMp
		//   byte  skillId
		//   byte  skillLevel
		int32_t oid = recv.read_int();
		recv.read_short();				// move id, ours, echoed back
		bool useskills = recv.read_bool();
		recv.read_short();				// the mob's current MP

		int8_t skill_id = recv.read_byte();
		int8_t skill_level = recv.read_byte();

		// useSkills false means "not this time" - the server sends zeroes with
		// it, but do not rely on that.
		if (!useskills)
			skill_id = skill_level = 0;

		Stage::get().get_mobs().grant_skill(oid, skill_id, skill_level);
	}

	void KillMobHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int8_t animation = recv.read_byte();

		// The server writes the animation byte TWICE. Read the second one -
		// leaving it behind is harmless in itself, but a handler that stops
		// short of the end of its packet is exactly the shape of a parser
		// that has drifted, and every one of them should be accounted for.
		recv.read_byte();

		Stage::get().get_mobs().remove(oid, animation);
	}

	void SpawnMobControllerHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();
		int32_t oid = recv.read_int();

		if (mode == 0)
		{
			Stage::get().get_mobs().set_control(oid, false);
		}
		else
		{
			if (recv.available())
			{
				recv.skip(1);

				int32_t id = recv.read_int();

				// Same 16 byte status block as SpawnMobHandler above.
				recv.skip(16);

				Point<int16_t> position = recv.read_point();
				int8_t stance = recv.read_byte();

				recv.skip(2);

				uint16_t fh = recv.read_short();

				// Same effect-then-marker layout as SpawnMobHandler above.
				int8_t effect = recv.read_byte();
				int8_t newspawn = read_spawn_marker(recv, effect);

				int8_t team = recv.read_byte();

				recv.skip(4);

				Stage::get().get_mobs().spawn(
					{ oid, id, mode, stance, fh, newspawn == -2, team, position }
				);
			}
			else
			{
				// TODO: Remove monster invisibility, not used (maybe in an event script?), Check this!
			}
		}
	}

	void MobMovedHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();

		recv.read_byte();
		recv.read_byte(); // useskill
		recv.read_byte(); // skill
		recv.read_byte(); // skill 1
		recv.read_byte(); // skill 2
		recv.read_byte(); // skill 3
		recv.read_byte(); // skill 4

		Point<int16_t> position = recv.read_point();
		std::vector<Movement> movements = MovementParser::parse_movements(recv);

		Stage::get().get_mobs().send_movement(oid, position, std::move(movements));
	}

	void ShowMobHpHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int8_t hppercent = recv.read_byte();
		uint16_t playerlevel = Stage::get().get_player().get_stats().get_stat(Maplestat::LEVEL);

		Stage::get().get_mobs().send_mobhp(oid, hppercent, playerlevel);
	}

	void SpawnNpcHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int32_t id = recv.read_int();
		Point<int16_t> position = recv.read_point();
		bool flip = recv.read_bool();
		uint16_t fh = recv.read_short();

		recv.read_short(); // 'rx'
		recv.read_short(); // 'ry'

		Stage::get().get_npcs().spawn(
			{ oid, id, position, flip, fh }
		);
	}

	void SpawnNpcControllerHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();
		int32_t oid = recv.read_int();

		if (mode == 0)
		{
			Stage::get().get_npcs().remove(oid);
		}
		else
		{
			int32_t id = recv.read_int();
			Point<int16_t> position = recv.read_point();
			bool flip = recv.read_bool();
			uint16_t fh = recv.read_short();

			recv.read_short();	// 'rx'
			recv.read_short();	// 'ry'
			recv.read_bool();	// 'minimap'

			Stage::get().get_npcs().spawn(
				{ oid, id, position, flip, fh }
			);
		}
	}

	void DropLootHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();
		int32_t oid = recv.read_int();
		bool meso = recv.read_bool();
		int32_t itemid = recv.read_int();
		int32_t owner = recv.read_int();
		int8_t pickuptype = recv.read_byte();
		Point<int16_t> dropto = recv.read_point();

		recv.skip(4);

		Point<int16_t> dropfrom;

		if (mode != 2)
		{
			dropfrom = recv.read_point();

			recv.skip(2);

			Sound(Sound::Name::DROP).play();
		}
		else
		{
			dropfrom = dropto;
		}

		if (!meso)
			recv.skip(8);

		bool playerdrop = !recv.read_bool();

		Stage::get().get_drops().spawn(
			{ oid, itemid, meso, owner, dropfrom, dropto, pickuptype, mode, playerdrop }
		);
	}

	void RemoveLootHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();
		int32_t oid = recv.read_int();

		Optional<PhysicsObject> looter;

		if (mode > 1)
		{
			int32_t cid = recv.read_int();

			if (recv.length() > 0)
				recv.read_byte(); // pet
			else if (auto character = Stage::get().get_character(cid))
				looter = character->get_phobj();

			Sound(Sound::Name::PICKUP).play();
		}

		Stage::get().get_drops().remove(oid, mode, looter.get());
	}

	void HitReactorHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int8_t state = recv.read_byte();
		Point<int16_t> point = recv.read_point();
		int8_t stance = recv.read_byte(); // TODO: When is this different than state?
		recv.skip(2); // TODO: Unused
		recv.skip(1); // "frame" delay but this is in the wz file?

		Stage::get().get_reactors().trigger(oid, state);
	}

	void SpawnReactorHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int32_t rid = recv.read_int();
		int8_t state = recv.read_byte();
		Point<int16_t> point = recv.read_point();

		// TODO: Unused, Check this!
		// uint16_t fhid = recv.read_short();
		// recv.read_byte()

		Stage::get().get_reactors().spawn(
			{ oid, rid, state, point }
		);
	}

	void RemoveReactorHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int8_t state = recv.read_byte();
		Point<int16_t> point = recv.read_point();

		Stage::get().get_reactors().remove(oid, state, point);
	}

	void SpawnDoorHandler::handle(InPacket& recv) const
	{
		bool launched = recv.read_bool();
		int32_t owner_id = recv.read_int();
		Point<int16_t> pos = recv.read_point();

		// A door is keyed by its owner rather than by an object id of its
		// own: the server identifies it that way when removing it, and a
		// character can only have one open at a time.
		Stage::get().get_doors().spawn(
			{ owner_id, owner_id, pos, launched }
		);
	}

	void RemoveDoorHandler::handle(InPacket& recv) const
	{
		recv.read_byte();	// always 0
		int32_t owner_id = recv.read_int();

		Stage::get().get_doors().remove(owner_id);
	}

	void SpawnMistHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int32_t mist_type = recv.read_int();	// 0 mob, 1 poison, 2 smokescreen, 4 recovery
		int32_t owner_id = recv.read_int();
		int32_t skill_id = recv.read_int();
		int8_t skill_level = recv.read_byte();

		recv.read_short();	// delay

		int32_t x1 = recv.read_int();
		int32_t y1 = recv.read_int();
		int32_t x2 = recv.read_int();
		int32_t y2 = recv.read_int();

		recv.read_int();	// unknown

		Point<int16_t> pos1(static_cast<int16_t>(x1), static_cast<int16_t>(y1));
		Point<int16_t> pos2(static_cast<int16_t>(x2), static_cast<int16_t>(y2));

		Stage::get().get_mists().spawn(
			{ oid, owner_id, pos1, pos2, skill_id, skill_level, static_cast<int8_t>(mist_type) }
		);
	}

	void RemoveMistHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();

		Stage::get().get_mists().remove(oid);
	}

	void SpawnSummonHandler::handle(InPacket& recv) const
	{
		int32_t owner_id = recv.read_int();
		int32_t oid = recv.read_int();
		int32_t skill_id = recv.read_int();

		recv.skip(1);	// 0x0A marker

		int8_t skill_level = recv.read_byte();
		Point<int16_t> position = recv.read_point();
		int8_t stance = recv.read_byte();

		recv.skip(2);	// padding

		int8_t move_type_val = recv.read_byte();
		bool attacks = recv.read_bool();

		Summon::MovementType move_type;

		switch (move_type_val)
		{
		case 1:
			move_type = Summon::MovementType::FOLLOW;
			break;
		case 3:
			move_type = Summon::MovementType::CIRCLE_FOLLOW;
			break;
		default:
			move_type = Summon::MovementType::STATIONARY;
			break;
		}

		Stage::get().get_summons().spawn(
			{ oid, owner_id, skill_id, skill_level, stance, position, move_type, attacks }
		);
	}

	void RemoveSummonHandler::handle(InPacket& recv) const
	{
		recv.read_int();	// owner
		int32_t oid = recv.read_int();
		int8_t anim = recv.read_byte();

		Stage::get().get_summons().remove(oid, anim == 4);
	}

	void MoveSummonHandler::handle(InPacket& recv) const
	{
		recv.read_int();	// owner
		int32_t oid = recv.read_int();
		Point<int16_t> start = recv.read_point();

		std::vector<Movement> movements = MovementParser::parse_movements(recv);

		Stage::get().get_summons().send_movement(oid, start, std::move(movements));
	}

	void SummonAttackHandler::handle(InPacket& recv) const
	{
		// Read but not acted on. The damage itself reaches us through the
		// mobs' own HP updates, so the only job here is to consume the packet
		// rather than let it be reported as unhandled.
		recv.read_int();	// owner
		recv.read_int();	// summon oid

		recv.skip(1);		// character level
		recv.skip(1);		// direction

		int8_t num_targets = recv.read_byte();

		for (int8_t i = 0; i < num_targets; i++)
		{
			recv.read_int();	// monster oid
			recv.skip(1);
			recv.read_int();	// damage
		}
	}

	void DamageSummonHandler::handle(InPacket& recv) const
	{
		recv.read_int();	// owner
		int32_t oid = recv.read_int();

		recv.skip(1);

		int32_t damage = recv.read_int();

		Stage::get().get_summons().apply_damage(oid, damage);
	}

	void SummonSkillHandler::handle(InPacket& recv) const
	{
		// Purely the summon's own skill animation, which is not drawn yet.
		recv.read_int();	// cid
		recv.read_int();	// skill id
		recv.read_byte();	// new stance
	}
}
