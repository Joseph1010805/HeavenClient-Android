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

#include "MapObject.h"

#include "../Movement.h"

#include "../Combat/Attack.h"
#include "../Combat/Bullet.h"
#include "../Audio/Audio.h"
#include "../Graphics/EffectLayer.h"
#include "../Graphics/Geometry.h"
#include "../Util/Randomizer.h"
#include "../Util/TimedBool.h"

namespace ms
{
	class Mob : public MapObject
	{
	public:
		static const size_t NUM_STANCES = 6;

		enum Stance : uint8_t
		{
			MOVE = 2,
			STAND = 4,
			JUMP = 6,
			HIT = 8,
			DIE = 10
		};

		static std::string nameof(Stance stance)
		{
			static const std::string stancenames[NUM_STANCES] =
			{
				"move", "stand", "jump", "hit1", "die1", "fly"
			};

			size_t index = (stance - 1) / 2;

			return stancenames[index];
		}

		static uint8_t value_of(Stance stance, bool flip)
		{
			return flip ? stance : stance + 1;
		}

		// Construct a mob by combining data from game files with data sent by the server.
		Mob(int32_t oid, int32_t mobid, int8_t mode, int8_t stance, uint16_t fhid, bool newspawn, int8_t team, Point<int16_t> position);

		// Draw the mob.
		void draw(double viewx, double viewy, float alpha) const override;
		// Update movement and animations.
		int8_t update(const Physics& physics) override;

		// Change this mob's control mode:
		// 0 - no control, 1 - control, 2 - aggro
		void set_control(int8_t mode);
		// Send movement to the mob.
		void send_movement(Point<int16_t> start, std::vector<Movement>&& movements);
		// Kill the mob with the appropriate type:
		// 0 - make inactive 1 - death animation 2 - fade out
		void kill(int8_t killtype);
		// Display the hp percentage above the mob.
		// Use the playerlevel to determine color of nametag.
		void show_hp(int8_t percentage, uint16_t playerlevel);
		// Show an effect at the mob's position.
		void show_effect(const Animation& animation, int8_t pos, int8_t z, bool flip);

		// Calculate the damage to this mob with the specified attack.
		std::vector<std::pair<int32_t, bool>> calculate_damage(const Attack& attack);
		// Apply damage to the mob.
		void apply_damage(int32_t damage, bool toleft);

		// Create a touch damage attack to the player.
		MobAttack create_touch_attack() const;

		// A swing that has finished and not yet been resolved. `target` is the
		// player's position; the attack only lands if it falls inside the
		// attack's own range box. Clears the pending hit either way - a miss
		// is resolved too.
		// Tell this mob where the player is. Only acted on while aggressive.
		void set_target(Point<int16_t> position);

		// Bosses drive the wide gauge across the top of the screen. The two
		// colours are Nexon's own choice per mob, from hpTagColor and
		// hpTagBgcolor in the mob's data.
		bool is_boss() const;
		std::string get_name() const;
		int8_t get_hp_tag_color() const;
		int8_t get_hp_tag_bgcolor() const;

		bool has_pending_hit() const;
		MobAttack take_pending_hit(Point<int16_t> target);
		int8_t pending_attack_index() const;
		// Remember the skill the server will allow on this mob's next move.
		// See next_skill_id.
		void grant_skill(int8_t skill_id, int8_t skill_level);

		// Check if this mob collides with the specified rectangle.
		bool is_in_range(const Rectangle<int16_t>& range) const;
		// Check if this mob is still alive.
		bool is_alive() const;
		// Return the head position.
		Point<int16_t> get_head_position() const;

	private:
		enum FlyDirection
		{
			STRAIGHT,
			UPWARDS,
			DOWNWARDS,
			NUM_DIRECTIONS
		};

		// Set the stance by byte value.
		void set_stance(uint8_t stancebyte);
		// Set the stance by enum value.
		void set_stance(Stance newstance);
		// Start the death animation.
		void apply_death();
		// Decide on the next state.
		void next_move(const Physics& physics);

		// IS THERE FLOOR THAT WAY?
		//
		// The turn-at-edges flag already stops a WANDERING monster walking
		// off - the physics halts it at the foothold's edge and Mob::update
		// flips it round. What it cannot do is stop a CHASING one, because
		// pursuit re-aims at the player on every decision and immediately
		// flips it back over the drop. So a mob that is following you needs
		// to ask the question itself, before it commits to a direction.
		bool ground_ahead(const Physics& physics, bool facing_right) const;

		// How far ahead a monster looks, and how far down counts as a drop
		// rather than a step. A Maple platform edge is a sheer fall; 60 is
		// well below any slope and well above any stair.
		static constexpr int16_t LOOK_AHEAD = 20;
		static constexpr int16_t CLIFF_DROP = 60;

		// How far above a monster the target has to be before it is worth
		// jumping at, and how far to either side it can be while it still
		// counts as overhead rather than "over there somewhere".
		// WHICH WAY THE JUMP WAS AIMED, decided at take-off and remembered.
		//
		// `flip` cannot be trusted for this. A mob standing at a platform
		// edge has TURNATEDGES cleared by the physics, and Mob::update turns
		// it round the very next frame - so between deciding to jump and the
		// jump being applied, the direction can already have been reversed
		// by the edge logic. The trace showed exactly that: the mushroom
		// jumped high enough every time and landed further away each time.
		//
		// -1 left, +1 right, 0 none.
		int8_t jump_dir = 0;

		// A MONSTER ONLY LEAVES ITS PLATFORM TO COME AFTER YOU.
		//
		// TURNATEDGES is what keeps a mob on its own foothold, and a jump has
		// to drop it or the physics clamps the mob to the ledge and it rises
		// and falls on the spot. So a jump is the ONE thing that can put a
		// monster over a drop - and a wandering monster jumps at random.
		//
		// Every one of those random hops was letting go of the ledge. A pig
		// that hopped near the lip walked off it, fell to the platform below,
		// hopped again, and so on down: given a few minutes every monster on
		// a layered map had migrated to the floor and piled up in a corner.
		// That is what "the enemies are all grouping at the bottom" was.
		//
		// `crossing` is the difference between a hop and a leap. It is set
		// ONLY for a jump taken while hunting, when the mob has decided to
		// get to a player who is above it or across a gap. A wandering hop
		// keeps the flag, so the physics holds it on its own ledge and it
		// stays where the map put it.
		bool crossing = false;

		// Take-off happens ONCE, on the frame the jump starts.
		//
		// The launch used to run every frame the stance was JUMP and the mob
		// was on the ground - and a mob that has landed is on the ground with
		// the stance still JUMP until it next thinks, which is up to a fifth
		// of a second. So it landed and immediately relaunched itself, over
		// and over, with the ledge released each time.
		bool jump_launch = false;

		static constexpr int16_t JUMP_UP = 40;
		static constexpr int16_t JUMP_REACH = 110;
		// Send the current position and state to the server.
		void update_movement();

		// Calculate the hit chance.
		float calculate_hitchance(int16_t leveldelta, int32_t accuracy) const;
		// Calculate the minimum damage.
		double calculate_mindamage(int16_t leveldelta, double mindamage, bool magic) const;
		// Calculate the maximum damage.
		double calculate_maxdamage(int16_t leveldelta, double maxdamage, bool magic) const;
		// Calculate a random damage line based on the specified values.
		std::pair<int32_t, bool> next_damage(double mindamage, double maxdamage, float hitchance, float critical) const;

		// Return the current 'head' position.
		Point<int16_t> get_head_position(Point<int16_t> position) const;

		std::map<Stance, Animation> animations;

		// CAST ANIMATIONS.
		//
		// Deliberately kept apart from `animations` rather than added to the
		// Stance enum. Stance feeds value_of(), and value_of() is written into
		// the movement fragment the server parses - so inventing new stance
		// numbers to hang an animation off would change what MOVE_MONSTER
		// means. A boss waving its arms is not worth risking the movement
		// protocol over.
		//
		// info/skill/N in the mob's own data gives { skill, level, action },
		// and `action` picks the skill<N> node to play. Keyed by action, since
		// several skills can share one animation - Papulatus has six skills
		// and four of them are action 3.
		struct SkillEntry
		{
			int32_t skill_id;
			int32_t level;
			int32_t action;
		};

		std::vector<SkillEntry> skill_table;
		std::map<int32_t, Animation> cast_animations;

		bool casting = false;
		int32_t cast_action = 0;

		// ATTACKS - attack1..N in the mob's data.
		//
		// A mob attack is entirely client-driven: the client decides the mob
		// swings, decides whether the player was inside `range` when it
		// connected, works out the damage and reports it with TAKE_DAMAGE.
		// The `index` here is what goes in that packet's `from` byte, which is
		// how the server finds the same attack in its own MobAttackInfo.
		struct AttackEntry
		{
			int8_t index;
			int32_t watk;
			int32_t matk;
			bool magic;
			int32_t conmp;
			int32_t after;                  // ms before this one may repeat
			Rectangle<int16_t> range;       // relative to the mob, unflipped
			Animation effect;               // played on the mob as it swings
			bool has_effect;

			// A ranged attack (info/type != 0) throws info/ball at you instead
			// of relying on the range box - Nependeath's seed, for instance.
			Animation ball;
			bool ranged;
		};

		std::vector<AttackEntry> attack_table;
		std::map<int8_t, Animation> attack_animations;

		// Where the player was as of this tick, so an aggressive mob can turn
		// toward them and walk at them. Set by MapMobs, which is the only
		// thing that knows both. A mob with no target wanders as before.
		Point<int16_t> target;
		bool has_target = false;

		// Milliseconds left of "somebody hit me". See apply_damage.
		int32_t provoked = 0;
		static constexpr int32_t PROVOKED_FOR = 8000;

		bool attacking = false;
		bool pending_hit = false;           // swing finished, damage not taken yet
		int8_t attack_index = 0;
		int32_t attack_cooldown = 0;        // ms remaining before the next swing

		// Projectiles in the air. A ranged attack does NOT resolve when its
		// animation ends - it resolves when the ball arrives, which may be a
		// second later and somewhere else entirely. That is also why a hit
		// from one skips the range box: the ball reaching you IS the hit test.
		std::vector<std::pair<int8_t, Bullet>> bullets;
		bool pending_projectile = false;

		// Which animation a granted skill should play, and the activity byte
		// that goes with it. Returns 0 when the mob has no entry for it.
		int32_t action_for_skill(int32_t skill_id, int32_t level) const;
		std::string name;
		Sound hitsound;
		Sound diesound;
		uint16_t level;
		float speed;
		float flyspeed;
		uint16_t watk;
		uint16_t matk;
		uint16_t wdef;
		uint16_t mdef;
		uint16_t accuracy;
		uint16_t avoid;
		uint16_t knockback;
		bool undead;
		bool touchdamage;
		bool noflip;
		bool notattack;
		bool canmove;
		bool canjump;
		bool canfly;
		bool boss = false;
		int8_t hp_tag_color = 0;
		int8_t hp_tag_bgcolor = 0;

		EffectLayer effects;
		Text namelabel;
		MobHpBar hpbar;
		Randomizer randomizer;

		TimedBool showhp;

		std::vector<Movement> movements;
		uint16_t counter;

		int32_t id;
		int8_t effect;
		int8_t team;
		bool dying;
		bool dead;
		bool control;
		bool aggro;

		// The skill the server has GRANTED this mob for its next move, from
		// MOVE_MOB_RESPONSE. Zero means none.
		//
		// The server picks the skill, not us - it rolls getRandomSkill(),
		// checks the HP threshold and the MP cost, and blanks it out if the
		// mob may not cast. All the client does is send back what it was
		// given. Cleared as soon as it is used, because the server only ever
		// grants one move ahead.
		int8_t next_skill_id = 0;
		int8_t next_skill_level = 0;

		Stance stance;
		bool flip;
		FlyDirection flydirection;
		float walkforce;
		int8_t hppercent;
		bool fading;
		bool fadein;
		Linear<float> opacity;
	};
}