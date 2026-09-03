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
#include "Mob.h"

#include "../Util/Misc.h"

#include "../Net/Packets/GameplayPackets.h"

#include <nlnx/nx.hpp>

namespace ms
{
	Mob::Mob(int32_t oi, int32_t mid, int8_t mode, int8_t st, uint16_t fh, bool newspawn, int8_t tm, Point<int16_t> position) : MapObject(oi)
	{
		std::string strid = string_format::extend_id(mid, 7);
		nl::node src = nl::nx::mob[strid + ".img"];

		nl::node info = src["info"];

		level = info["level"];
		watk = info["PADamage"];
		matk = info["MADamage"];
		wdef = info["PDDamage"];
		mdef = info["MDDamage"];
		accuracy = info["acc"];
		avoid = info["eva"];
		knockback = info["pushed"];
		speed = info["speed"];
		flyspeed = info["flySpeed"];
		touchdamage = info["bodyAttack"].get_bool();
		undead = info["undead"].get_bool();
		noflip = info["noFlip"].get_bool();
		notattack = info["notAttack"].get_bool();
		boss = info["boss"].get_bool();
		hp_tag_color = static_cast<int8_t>(static_cast<int32_t>(info["hpTagColor"]));
		hp_tag_bgcolor = static_cast<int8_t>(static_cast<int32_t>(info["hpTagBgcolor"]));
		canjump = src["jump"].size() > 0;
		canfly = src["fly"].size() > 0;
		canmove = src["move"].size() > 0 || canfly;

		if (canfly)
		{
			animations[Stance::STAND] = src["fly"];
			animations[Stance::MOVE] = src["fly"];
		}
		else
		{
			animations[Stance::STAND] = src["stand"];
			animations[Stance::MOVE] = src["move"];
		}

		animations[Stance::JUMP] = src["jump"];
		animations[Stance::HIT] = src["hit1"];
		animations[Stance::DIE] = src["die1"];

		// attack1, attack2, ... run until one is missing. The suffix is
		// 1-based in the data but the packet's `from` byte is 0-based, so the
		// index stored here is the packet's, not the node's.
		for (int8_t n = 0; n < 18; n++)
		{
			nl::node atk = src["attack" + std::to_string(n + 1)];

			if (atk.size() == 0)
				break;

			nl::node ainfo = atk["info"];

			AttackEntry ae;
			ae.index = n;
			ae.magic = ainfo["magic"].get_bool();
			ae.conmp = ainfo["conMP"];
			ae.after = ainfo["attackAfter"];

			// The attack may state its own damage; otherwise the mob's.
			int32_t pad = ainfo["PADamage"];
			int32_t mad = ainfo["MADamage"];
			ae.watk = pad > 0 ? pad : watk;
			ae.matk = mad > 0 ? mad : matk;

			ae.range = Rectangle<int16_t>(ainfo["range"]);

			nl::node eff = ainfo["effect"];
			ae.has_effect = eff.size() > 0;

			if (ae.has_effect)
				ae.effect = eff;

			// type 0 is a melee swing; anything else throws `ball`.
			nl::node ball = ainfo["ball"];
			ae.ranged = static_cast<int32_t>(ainfo["type"]) != 0 && ball.size() > 0;

			if (ae.ranged)
				ae.ball = ball;

			attack_table.push_back(ae);
			attack_animations[n] = atk;
		}

		// The mob's own skill table. Bosses carry one; ordinary monsters do
		// not, and skill_table simply stays empty for them.
		for (nl::node entry : info["skill"])
		{
			SkillEntry se;
			se.skill_id = entry["skill"];
			se.level = entry["level"];
			se.action = entry["action"];

			if (se.skill_id == 0)
				continue;

			skill_table.push_back(se);

			if (cast_animations.count(se.action) == 0)
			{
				nl::node anim = src["skill" + std::to_string(se.action)];

				if (anim.size() > 0)
					cast_animations[se.action] = anim;
			}
		}

		name = std::string(nl::nx::string["Mob.img"][std::to_string(mid)]["name"]);

		nl::node sndsrc = nl::nx::sound["Mob.img"][strid];

		hitsound = sndsrc["Damage"];
		diesound = sndsrc["Die"];

		speed += 100;
		speed *= 0.001f;

		flyspeed += 100;
		flyspeed *= 0.0005f;

		if (canfly)
			phobj.type = PhysicsObject::Type::FLYING;

		id = mid;
		team = tm;
		set_position(position);
		set_control(mode);
		phobj.fhid = fh;
		phobj.set_flag(PhysicsObject::Flag::TURNATEDGES);

		hppercent = 0;
		dying = false;
		dead = false;
		fading = false;
		set_stance(st);
		flydirection = STRAIGHT;
		counter = 0;

		// THE LEVEL, ON THE NAME TAG.
		//
		// It decides everything about a fight - the hit chance, the damage
		// both ways, and now whether the kill counts toward the daily hunt,
		// which only takes monsters within five levels of you. Leaving it to
		// be guessed from how hard something hits is a lot to ask.
		namelabel = Text(Text::Font::A13M, Text::Alignment::CENTER,
			Color::Name::WHITE, Text::Background::NAMETAG,
			"Lv." + std::to_string(level) + " " + name);

		if (newspawn)
		{
			fadein = true;
			opacity.set(0.0f);
		}
		else
		{
			fadein = false;
			opacity.set(1.0f);
		}

		// NO FIRST MOVE FROM THE CONSTRUCTOR.
		//
		// It used to pick one here, but choosing a direction now means asking
		// whether there is floor that way, and the physics for this map is not
		// something a mob has at the moment it is built. The first update is a
		// few milliseconds away and calls next_move itself, so nothing is lost
		// but the guess.
	}

	void Mob::set_stance(uint8_t stancebyte)
	{
		flip = (stancebyte % 2) == 0;

		if (!flip)
			stancebyte -= 1;

		if (stancebyte < Stance::MOVE)
			stancebyte = Stance::MOVE;

		set_stance(static_cast<Stance>(stancebyte));
	}

	void Mob::set_stance(Stance newstance)
	{
		if (stance != newstance)
		{
			stance = newstance;

			// Arm the take-off. The JUMP case in update() consumes this on
			// the next frame the mob has ground under it, and then the jump
			// is under way - it cannot fire again until something chooses to
			// jump afresh.
			if (stance == Stance::JUMP)
				jump_launch = true;

			animations.at(stance).reset();
		}
	}

	int8_t Mob::update(const Physics& physics)
	{
		if (!active)
			return phobj.fhlayer;

		// The cast runs alongside the stance animation rather than replacing
		// it, so movement and the server's idea of what this mob is doing are
		// untouched - only what gets drawn changes.
		if (casting)
		{
			auto found = cast_animations.find(cast_action);

			if (found == cast_animations.end() || found->second.update())
				casting = false;
		}

		if (attack_cooldown > 0)
			attack_cooldown -= Constants::TIMESTEP;

		if (provoked > 0)
			provoked -= Constants::TIMESTEP;

		if (attacking)
		{
			auto found = attack_animations.find(attack_index);

			if (found == attack_animations.end() || found->second.update())
			{
				// The swing is over. Whether it CONNECTED is not the mob's
				// business - it does not know where the player is. Stage asks
				// for the pending hit and decides.
				//
				// A ranged attack is the exception: its ball is already in the
				// air and will report for itself.
				attacking = false;

				bool ranged = false;
				for (const auto& ae : attack_table)
					if (ae.index == attack_index)
						ranged = ae.ranged;

				if (!ranged)
					pending_hit = true;
			}
		}

		// Balls in flight. They chase the player rather than a fixed point,
		// which is how the player's own bullets behave in Combat::update.
		if (has_target && !bullets.empty())
		{
			for (auto it = bullets.begin(); it != bullets.end(); )
			{
				if (it->second.update(target))
				{
					attack_index = it->first;
					pending_hit = true;
					pending_projectile = true;

					it = bullets.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		bool aniend = animations.at(stance).update();

		if (aniend && stance == Stance::DIE)
			dead = true;

		if (fading)
		{
			opacity -= 0.025f;

			if (opacity.last() < 0.025f)
			{
				opacity.set(0.0f);
				fading = false;
				dead = true;
			}
		}
		else if (fadein)
		{
			opacity += 0.025f;

			if (opacity.last() > 0.975f)
			{
				opacity.set(1.0f);
				fadein = false;
			}
		}

		if (dead)
		{
			deactivate();

			return -1;
		}

		effects.update();
		showhp.update();

		if (!dying)
		{
			// TURNING AT AN EDGE IS FOR WANDERING, NOT FOR HUNTING.
			//
			// The physics clears TURNATEDGES when something reaches the lip
			// of a foothold, and this turns it round. That is right for a
			// monster mooching about, and wrong for one that is chasing you:
			// standing at the edge NEAREST you is exactly where it wants to
			// be, and being spun to face away every frame is how it ended up
			// jumping away from the player it was hunting.
			//
			// A chasing mob keeps its own facing - see the steering below,
			// which aims it at the target every frame - and the flag is still
			// re-armed so the physics goes on holding it at the edge.
			bool hunting = (aggro || provoked > 0) && has_target && canmove;

			// AND NOT WHILE IT IS IN THE AIR.
			//
			// TURNATEDGES does not merely turn things round - Footholdtree::
			// limit_movement CLAMPS anything carrying it to the edge of its
			// foothold, and a jumping mob keeps the foothold it took off from
			// for the whole flight. Re-arming the flag every frame, which is
			// what the hunting change above did, therefore pinned the mob to
			// the platform edge IN MID-AIR: it launched with the right speed
			// in the right direction and was held at the same x until it came
			// down. The trace showed it exactly - dx stuck at -57 through
			// take-off, apex and landing.
			//
			// A JUMP IS OVER THE MOMENT THERE IS GROUND UNDERFOOT.
			//
			// Not when the mob next thinks. `crossing` used to be read off
			// the stance, and the stance stays JUMP after landing until the
			// decision timer comes round - a window in which the mob was on
			// the ground with the ledge released and walked straight off it.
			//
			// Cleared here, before the flag is re-armed below, so the mob
			// retakes its platform on the very frame it lands.
			if (phobj.onground && !jump_launch)
				crossing = false;

			// AND WHENEVER THE STANCE IS NOT A JUMP AT ALL.
			//
			// `jump_launch` is armed when JUMP is chosen and disarmed when
			// the launch fires. If anything moves the stance off JUMP in
			// between - and the SERVER can, every relayed monster movement
			// calls set_stance - the launch never fires, the flag is never
			// disarmed, and `crossing` above can never be cleared. From that
			// moment the mob has no TURNATEDGES and walks off every ledge it
			// meets, which is the exact behaviour this was written to stop.
			//
			// Deriving both from the stance closes it: not jumping, not
			// crossing, and nothing pending.
			if (stance != Stance::JUMP)
			{
				jump_launch = false;
				crossing = false;
			}

			// So the flag is only let go for a jump that is DELIBERATELY
			// crossing - see Mob.h. A wandering hop keeps it and is held on
			// its own foothold; only a hunting mob may leave.
			if (!canfly && phobj.onground && !crossing)
			{
				if (phobj.is_flag_not_set(PhysicsObject::Flag::TURNATEDGES))
				{
					if (!hunting)
						flip = !flip;

					phobj.set_flag(PhysicsObject::Flag::TURNATEDGES);

					if (stance == Stance::HIT)
						set_stance(Stance::STAND);
				}
			}

			// AIM EVERY FRAME, NOT EVERY DECISION.
			//
			// Which way a mob FACES while chasing is not a decision worth
			// taking four times a second - it is simply "where is he now".
			// Leaving it to next_move meant a monster walked the direction
			// you were in a moment ago, and the faster you moved the further
			// behind its aim ran. Stance, attacks and jumps stay on the
			// decision timer; only the steering is continuous.
			if (!canfly && canmove && stance == Stance::MOVE
				&& (aggro || provoked > 0) && has_target)
			{
				int16_t dx = static_cast<int16_t>(target.x() - get_position().x());

				if (dx > 20 || dx < -20)
				{
					bool want = dx > 0;

					// Only turn toward them if there is floor that way. The
					// edge check in next_move stops a mob SETTING OFF over a
					// drop; without this one it would still be steered off the
					// side between decisions.
					if (want == flip || ground_ahead(physics, want))
						flip = want;
				}
			}

			switch (stance)
			{
			case Stance::MOVE:
				if (canfly)
				{
					phobj.hforce = flip ? flyspeed : -flyspeed;

					switch (flydirection)
					{
					case FlyDirection::UPWARDS:
						phobj.vforce = -flyspeed;
						break;
					case FlyDirection::DOWNWARDS:
						phobj.vforce = flyspeed;
						break;
					}
				}
				else
				{
					// STOP AT THE LIP - ONLY WHILE HUNTING, AND ONLY ON THE
					// GROUND.
					//
					// `hunting` matters, and leaving it out cost a whole class
					// of monster. The physics ALREADY turns a wandering mob at
					// a foothold edge and always did; the only thing that
					// needed fixing was a CHASING one, which is re-aimed at
					// the player every frame and walked straight back over the
					// drop.
					//
					// Applied to wanderers as well, this froze any monster
					// whose platform was shorter than the look-ahead in both
					// directions - it stood in the middle of its own ledge
					// with nowhere it was willing to step. A level 12 Octopus
					// on a short foothold never moved again.
					//
					// `onground` is not a detail either. In the air there is
					// BY DEFINITION no floor ahead, so without it a monster
					// that jumped a gap lost its forward force the instant it
					// left the ground and dropped into the gap it was
					// clearing.
					if (canmove && hunting && phobj.onground
						&& !ground_ahead(physics, flip))
						phobj.hforce = 0.0;
					else
						phobj.hforce = flip ? speed : -speed;
				}

				break;
			case Stance::HIT:
				if (canmove)
				{
					// How far a monster is pushed by the player's hit. Halved from
				// 0.2/0.1 - like the walk force these are the client's own
				// approximation of v83, and monsters slid much too far.
				double KBFORCE = phobj.onground ? 0.1 : 0.05;
					phobj.hforce = flip ? -KBFORCE : KBFORCE;
				}

				break;
			case Stance::JUMP:
				phobj.vforce = -5.0;

				// AND FORWARD - AS A SPEED, NOT A FORCE.
				//
				// Setting hforce here did almost nothing, and it took reading
				// Physics::move_normal to see why: forces are only applied
				// `if (phobj.onground)`. So a force set during a jump lands
				// for exactly one frame at take-off and is ignored for the
				// whole flight. One frame of 0.1 acceleration is not a leap,
				// it is a twitch.
				//
				// Player::get_walkforce has the conversion: terminal speed is
				// 7.5 x hforce, in pixels per step. A pig's hforce is 0.1, so
				// it walks at 0.75 px/step - and a mob that jumped from
				// STANDING carried none of that, because it had none.
				//
				// So the speed is set outright, once, at take-off. 1.5x its
				// own walking speed: enough to clear a gap, still slower for
				// a snail than for a pig, and it comes back to zero on
				// landing like any other speed.
				// ONCE. `jump_launch` is set when the stance is chosen and
				// cleared here, so a mob that has landed but has not yet
				// re-thought cannot relaunch itself frame after frame.
				if (jump_launch && canmove && phobj.onground)
				{
					jump_launch = false;

					phobj.hspeed = (jump_dir != 0 ? jump_dir : (flip ? 1 : -1))
						* speed * 7.5 * 1.5;

					// LET GO OF THE LEDGE - BUT ONLY IF THIS JUMP IS MEANT TO
					// LEAVE IT.
					//
					// Without this the clamp above holds the mob at the edge
					// for the whole jump and it rises and falls on the spot,
					// which is right for a monster amusing itself and wrong
					// for one coming after you.
					if (crossing)
						phobj.clear_flag(PhysicsObject::Flag::TURNATEDGES);
				}

				break;
			}

			physics.move_object(phobj);

			if (control)
			{
				counter++;

				// Temporary: a controlled monster picks its next move when the
				// animation ends and the counter has run out. Report both, so
				// whichever is stuck is visible rather than guessed at.
				if (counter % 500 == 0)
					printf("[*] mob %d: stance %d canmove %d aniend %d counter %d onground %d\n",
						oid, static_cast<int>(stance), canmove ? 1 : 0,
						aniend ? 1 : 0, counter, phobj.onground ? 1 : 0);

				// How long before this mob reconsiders. 200 ticks at 125 a
				// second is 1.6 seconds, which is why monsters looked asleep -
				// an aggressive one re-decides four times faster, so it can
				// actually follow a player who is moving.
				uint16_t patience = (aggro || provoked > 0) ? 50 : 200;

				bool next;

				switch (stance)
				{
				case Stance::HIT:
					// NOT `patience`. apply_damage sets counter to 170 and the
					// flinch is what burns the rest off - so shortening
					// patience for an aggressive mob cancelled the reaction
					// almost the instant it started, and monsters stopped
					// visibly reacting to being hit. Being knocked about is a
					// fixed length, whatever mood the mob is in.
					next = counter > 200;
					break;
				case Stance::JUMP:
					// NOT while the take-off is still armed. A mob decides to
					// jump with its feet on the floor, so on the very next
					// frame `onground` is still true and this ended the jump
					// before it had begun - the stance changed away from JUMP
					// and the launch below never ran at all. Which is most of
					// why a hunting mob sometimes just stood at the edge.
					next = phobj.onground && !jump_launch;

					// Landed - the aim belonged to that jump only.
					if (next)
						jump_dir = 0;

					break;
				default:
					// CHASING DOES NOT WAIT FOR THE ANIMATION.
					//
					// `aniend` means the walk cycle has looped, and a slow
					// monster's walk cycle is over a second - so however low
					// `patience` went, a chasing mob could still only change
					// its mind at the end of a stride. That is the whole of
					// why the mushrooms follow you so badly: not that they
					// decide the wrong thing, that they decide it far too
					// rarely to keep up with somebody walking.
					next = (aggro || provoked > 0)
						? counter > patience
						: (aniend && counter > patience);
					break;
				}

				// A swing owns the mob until it finishes.
				if (attacking || casting)
					next = false;

				if (next)
				{
					next_move(physics);
					update_movement();
					counter = 0;
				}
			}
		}
		else
		{
			phobj.normalize();
			physics.get_fht().update_fh(phobj);
		}

		return phobj.fhlayer;
	}

	bool Mob::ground_ahead(const Physics& physics, bool facing_right) const
	{
		Point<int16_t> here = get_position();

		int16_t probe = static_cast<int16_t>(
			here.x() + (facing_right ? LOOK_AHEAD : -LOOK_AHEAD));

		// Started a little ABOVE the feet. Asking from exactly the foot line
		// on a sloped foothold finds the slope itself and answers "no floor"
		// on perfectly good ground.
		int16_t ground = physics.get_fht().get_y_below(
			Point<int16_t>(probe, static_cast<int16_t>(here.y() - 12)));

		return ground - here.y() < CLIFF_DROP;
	}

	void Mob::next_move(const Physics& physics)
	{
		// A swing takes priority over wandering, but only while this mob is
		// aggressive - otherwise every monster on the map would stand around
		// slashing at nothing, which is both wrong and noisy.
		if ((aggro || provoked > 0) && !notattack && !attack_table.empty()
			&& attack_cooldown <= 0 && !attacking && !casting)
		{
			const AttackEntry& ae =
				attack_table[randomizer.next_int(attack_table.size())];

			attack_index = ae.index;
			attack_cooldown = ae.after > 0 ? ae.after : 1000;
			attacking = true;

			// Face what you are swinging at, or the range box mirrors away
			// from the player and every attack misses.
			if (has_target)
				flip = target.x() > get_position().x();

			auto found = attack_animations.find(attack_index);

			if (found != attack_animations.end())
			{
				found->second.reset();

				if (ae.has_effect)
					effects.add(ae.effect, DrawArgument(flip && !noflip));

				// Ranged attacks put a ball in the air now; it resolves when
				// it lands, not when this animation ends.
				if (ae.ranged && has_target)
				{
					Bullet ball(ae.ball, get_head_position(), !flip);

					// settarget answers true when the target is already close
					// enough that the ball has nowhere to travel.
					if (ball.settarget(target))
					{
						pending_hit = true;
						pending_projectile = true;
					}
					else
					{
						bullets.emplace_back(ae.index, ball);
					}
				}

				return;
			}

			attacking = false;
		}

		// PURSUIT.
		//
		// An aggressive mob walks at the player instead of picking a direction
		// out of the air. Without this a monster can only hurt you by standing
		// where you happen to be, which is why they read as scenery.
		if ((aggro || provoked > 0) && has_target && canmove)
		{
			int16_t dx = static_cast<int16_t>(target.x() - get_position().x());

			// Close enough - stop shuffling and stand, so the mob does not
			// jitter left and right on top of the player.
			//
			// UNLESS THEY ARE ABOVE YOU. This is why a pig stood under a
			// platform doing nothing: standing directly beneath the player is
			// "close enough" by every horizontal measure, so it never reached
			// the jump decision at all. Being under somebody is not being
			// near them.
			int16_t rise = static_cast<int16_t>(target.y() - get_position().y());

			if (rise > -JUMP_UP && dx > -20 && dx < 20)
			{
				set_stance(Stance::STAND);
			}
			else
			{
				flip = dx > 0;

				if (canfly)
				{
					set_stance(Stance::MOVE);

					int16_t dy = static_cast<int16_t>(target.y() - get_position().y());

					flydirection = (dy < -20) ? FlyDirection::UPWARDS
						: (dy > 20) ? FlyDirection::DOWNWARDS
						: FlyDirection::STRAIGHT;

					return;
				}

				int16_t dy = static_cast<int16_t>(target.y() - get_position().y());

				bool floor = ground_ahead(physics, flip);

				// UP ONTO YOUR PLATFORM.
				//
				// Not a dice roll any more. A monster that can jump does it
				// when the player is ABOVE it and roughly overhead - which is
				// the whole of "a pig follows you onto the ledge" - and when
				// there is a gap in the way. It was a flat 10% chance before,
				// so a pig under a platform would eventually hop for reasons
				// unconnected to where you were standing.
				bool wants_up = dy < -JUMP_UP
					&& (dx > -JUMP_REACH && dx < JUMP_REACH);

				if (canjump && phobj.onground && (wants_up || !floor))
				{
					// AIMED HERE, USED ON TAKE-OFF. dx is the truth about
					// where the player is; `flip` is a rendering detail that
					// something else may have flipped by then.
					jump_dir = dx > 0 ? 1 : -1;
					flip = dx > 0;

					// THE ONE PLACE THIS IS TRUE. Hunting, on the ground, and
					// either the player is above or there is a gap in the way
					// - which between them are the only reasons a monster has
					// to be anywhere but on its own platform.
					crossing = true;

					set_stance(Stance::JUMP);

					return;
				}

				// NO FLOOR AND NO JUMP: STAY.
				//
				// A monster that cannot jump stands at the edge facing you
				// rather than stepping off it. Standing is deliberate - it
				// keeps looking at you, so it is plainly still hunting and
				// still dangerous if you come back, rather than wandering off
				// and reading as bored.
				if (!floor)
				{
					set_stance(Stance::STAND);

					return;
				}

				set_stance(Stance::MOVE);
			}

			return;
		}

		if (canmove)
		{
			switch (stance)
			{
			case Stance::HIT:
			case Stance::STAND:
				set_stance(Stance::MOVE);
				flip = randomizer.next_bool();
				break;
			case Stance::MOVE:
			case Stance::JUMP:
				if (canjump && phobj.onground && randomizer.below(0.25f))
				{
					// A HOP, NOT A LEAP. Nobody is being chased, so the mob
					// keeps its ledge and the physics holds it there.
					crossing = false;

					set_stance(Stance::JUMP);
				}
				else
				{
					switch (randomizer.next_int(3))
					{
					case 0:
						set_stance(Stance::STAND);
						break;
					case 1:
						set_stance(Stance::MOVE);
						flip = false;
						break;
					case 2:
						set_stance(Stance::MOVE);
						flip = true;
						break;
					}

					// NO EDGE CHECK WHEN WANDERING. See the note in update().

				}

				break;
			}

			if (stance == Stance::MOVE && canfly)
				flydirection = randomizer.next_enum(FlyDirection::NUM_DIRECTIONS);
		}
		else
		{
			set_stance(Stance::STAND);
		}
	}

	void Mob::grant_skill(int8_t skill_id, int8_t skill_level)
	{
		next_skill_id = skill_id;
		next_skill_level = skill_level;
	}

	int32_t Mob::action_for_skill(int32_t skill_id, int32_t level) const
	{
		// Exact match first - a mob can carry the same skill at several levels
		// with a different animation for each (Papulatus lists skill 200 at
		// levels 39 and 40).
		for (const auto& se : skill_table)
			if (se.skill_id == skill_id && se.level == level)
				return se.action;

		for (const auto& se : skill_table)
			if (se.skill_id == skill_id)
				return se.action;

		return 0;
	}

	void Mob::update_movement()
	{
		// The three bytes after the move id are, on Cosmic's side:
		//
		//     skillb -> pNibbles      must be 0 or the server will not offer
		//                             a skill on the next move
		//     skill0 -> rawActivity   24-41 is an attack, 42-59 is a skill
		//     skill1 -> skillId       and skill2 -> skillLv
		//
		// See MoveLifeHandler.handlePacket. Sending 0 for all of them - which
		// is what this did until now - means rawActivity is never in the skill
		// range, so no monster in the game has ever cast anything.
		int8_t activity = 0;
		int8_t skill_id = 0;
		int8_t skill_level = 0;

		if (next_skill_id != 0)
		{
			// 42 is the first skill slot, and the mob's own `action` picks
			// which one - so the activity byte matches the animation being
			// played. The server only range-checks it (the skill itself comes
			// from the two bytes after), but sending the real slot keeps the
			// packet honest.
			int32_t action = action_for_skill(next_skill_id, next_skill_level);

			activity = static_cast<int8_t>(42 + (action > 0 ? action - 1 : 0));
			skill_id = next_skill_id;
			skill_level = next_skill_level;

			// Play the cast. Nothing else drives this - the server tells us
			// what to cast, so this is the only place that knows one is
			// happening.
			auto found = cast_animations.find(action);

			if (found != cast_animations.end())
			{
				casting = true;
				cast_action = action;
				found->second.reset();
			}

			// One grant, one cast. The server hands out permission a single
			// move ahead, and re-granting is its job, not ours.
			next_skill_id = 0;
			next_skill_level = 0;
		}

		MoveMobPacket(
			oid, 1, 0, activity, skill_id, skill_level, 0, 0, get_position(),
			Movement(phobj, value_of(stance, flip))
		).dispatch();
	}

	void Mob::draw(double viewx, double viewy, float alpha) const
	{
		Point<int16_t> absp = phobj.get_absolute(viewx, viewy, alpha);
		Point<int16_t> headpos = get_head_position(absp);

		effects.drawbelow(absp, alpha);

		if (!dead)
		{
			float interopc = opacity.get(alpha);

			DrawArgument how(absp, flip && !noflip, interopc);

			auto cast = casting ? cast_animations.find(cast_action) : cast_animations.end();
			auto swing = attacking ? attack_animations.find(attack_index) : attack_animations.end();

			if (cast != cast_animations.end())
				cast->second.draw(how, alpha);
			else if (swing != attack_animations.end())
				swing->second.draw(how, alpha);
			else
				animations.at(stance).draw(how, alpha);

			if (showhp)
			{
				namelabel.draw(absp);

				if (!dying && hppercent > 0)
					hpbar.draw(headpos, hppercent);
			}
		}

		effects.drawabove(absp, alpha);

		// Balls take the raw view offsets, not the mob's screen position -
		// they have their own position in the world and move independently of
		// whatever threw them.
		for (const auto& shot : bullets)
			shot.second.draw(viewx, viewy, alpha);
	}

	void Mob::set_control(int8_t mode)
	{
		control = mode > 0;
		aggro = mode == 2;
	}

	void Mob::send_movement(Point<int16_t> start, std::vector<Movement>&& in_movements)
	{
		if (control)
			return;

		set_position(start);

		movements = std::forward<decltype(in_movements)>(in_movements);

		if (movements.empty())
			return;

		const Movement& lastmove = movements.front();

		uint8_t laststance = lastmove.newstate;
		set_stance(laststance);

		phobj.fhid = lastmove.fh;
	}

	Point<int16_t> Mob::get_head_position(Point<int16_t> position) const
	{
		Point<int16_t> head = animations.at(stance).get_head();

		position.shift_x((flip && !noflip) ? -head.x() : head.x());
		position.shift_y(head.y());

		return position;
	}

	void Mob::kill(int8_t animation)
	{
		switch (animation)
		{
		case 0:
			deactivate();
			break;
		case 1:
			dying = true;

			apply_death();
			break;
		case 2:
			fading = true;
			dying = true;
			break;
		}
	}

	void Mob::show_hp(int8_t percent, uint16_t playerlevel)
	{
		if (hppercent == 0)
		{
			int16_t delta = playerlevel - level;

			if (delta > 9)
				namelabel.change_color(Color::Name::YELLOW);
			else if (delta < -9)
				namelabel.change_color(Color::Name::RED);
		}

		if (percent > 100)
			percent = 100;
		else if (percent < 0)
			percent = 0;

		hppercent = percent;
		showhp.set_for(2000);
	}

	void Mob::show_effect(const Animation& animation, int8_t pos, int8_t z, bool f)
	{
		if (!active)
			return;

		Point<int16_t> shift;

		switch (pos)
		{
		case 0:
			shift = get_head_position(Point<int16_t>());
			break;
		case 1:
			break;
		case 2:
			break;
		case 3:
			break;
		case 4:
			break;
		}

		effects.add(animation, DrawArgument(shift, f), z);
	}

	float Mob::calculate_hitchance(int16_t leveldelta, int32_t player_accuracy) const
	{
		float faccuracy = static_cast<float>(player_accuracy);
		float hitchance = faccuracy / (((1.84f + 0.07f * leveldelta) * avoid) + 1.0f);

		if (hitchance < 0.01f)
			hitchance = 0.01f;

		return hitchance;
	}

	double Mob::calculate_mindamage(int16_t leveldelta, double damage, bool magic) const
	{
		double mindamage =
			magic ?
			damage - (1 + 0.01 * leveldelta) * mdef * 0.6 :
			damage * (1 - 0.01 * leveldelta) - wdef * 0.6;

		return mindamage < 1.0 ? 1.0 : mindamage;
	}

	double Mob::calculate_maxdamage(int16_t leveldelta, double damage, bool magic) const
	{
		double maxdamage =
			magic ?
			damage - (1 + 0.01 * leveldelta) * mdef * 0.5 :
			damage * (1 - 0.01 * leveldelta) - wdef * 0.5;

		return maxdamage < 1.0 ? 1.0 : maxdamage;
	}

	std::vector<std::pair<int32_t, bool>> Mob::calculate_damage(const Attack& attack)
	{
		double mindamage;
		double maxdamage;
		float hitchance;
		float critical;
		int16_t leveldelta = level - attack.playerlevel;

		if (leveldelta < 0)
			leveldelta = 0;

		Attack::DamageType damagetype = attack.damagetype;

		switch (damagetype)
		{
		case Attack::DamageType::DMG_WEAPON:
		case Attack::DamageType::DMG_MAGIC:
			mindamage = calculate_mindamage(leveldelta, attack.mindamage, damagetype == Attack::DamageType::DMG_MAGIC);
			maxdamage = calculate_maxdamage(leveldelta, attack.maxdamage, damagetype == Attack::DamageType::DMG_MAGIC);
			hitchance = calculate_hitchance(leveldelta, attack.accuracy);
			critical = attack.critical;
			break;
		case Attack::DamageType::DMG_FIXED:
			mindamage = attack.fixdamage;
			maxdamage = attack.fixdamage;
			hitchance = 1.0f;
			critical = 0.0f;
			break;
		}

		std::vector<std::pair<int32_t, bool>> result(attack.hitcount);

		std::generate(
			result.begin(), result.end(),
			[&]()
			{
				return next_damage(mindamage, maxdamage, hitchance, critical);
			}
		);

		update_movement();

		return result;
	}

	std::pair<int32_t, bool> Mob::next_damage(double mindamage, double maxdamage, float hitchance, float critical) const
	{
		bool hit = randomizer.below(hitchance);

		if (!hit)
			return std::pair<int32_t, bool>(0, false);

		constexpr double DAMAGECAP = 999999.0;

		double damage = randomizer.next_real(mindamage, maxdamage);
		bool iscritical = randomizer.below(critical);

		if (iscritical)
			damage *= 1.5;

		if (damage < 1)
			damage = 1;
		else if (damage > DAMAGECAP)
			damage = DAMAGECAP;

		auto intdamage = static_cast<int32_t>(damage);

		return std::pair<int32_t, bool>(intdamage, iscritical);
	}

	void Mob::apply_damage(int32_t damage, bool toleft)
	{
		hitsound.play();

		// BEING HIT MAKES IT CHASE YOU.
		//
		// Pursuit was gated purely on `aggro`, which is control mode 2 and
		// only ever set for mobs the server considers aggressive. Passive
		// monsters - snails, the first thing anyone attacks - are mode 1, so
		// they stood there when hit, which is not how the game behaves.
		//
		// Timed rather than permanent so a mob that loses you goes back to
		// wandering instead of following forever.
		provoked = PROVOKED_FOR;

		if (dying && stance != Stance::DIE)
		{
			apply_death();
		}
		else if (control && is_alive() && damage >= knockback)
		{
			flip = toleft;
			counter = 170;
			set_stance(Stance::HIT);

			update_movement();
		}
	}

	MobAttack Mob::create_touch_attack() const
	{
		if (!touchdamage)
			return MobAttack();

		int32_t minattack = static_cast<int32_t>(watk * 0.8f);
		int32_t maxattack = watk;
		int32_t attack = randomizer.next_int(minattack, maxattack);

		return MobAttack(attack, get_position(), id, oid);
	}

	bool Mob::is_boss() const
	{
		return boss;
	}

	std::string Mob::get_name() const
	{
		return name;
	}

	int8_t Mob::get_hp_tag_color() const
	{
		return hp_tag_color;
	}

	int8_t Mob::get_hp_tag_bgcolor() const
	{
		return hp_tag_bgcolor;
	}

	void Mob::set_target(Point<int16_t> position)
	{
		target = position;
		has_target = true;
	}

	bool Mob::has_pending_hit() const
	{
		return pending_hit && active && !dying;
	}

	int8_t Mob::pending_attack_index() const
	{
		return attack_index;
	}

	MobAttack Mob::take_pending_hit(Point<int16_t> target)
	{
		pending_hit = false;

		bool by_projectile = pending_projectile;
		pending_projectile = false;

		for (const auto& ae : attack_table)
		{
			if (ae.index != attack_index)
				continue;

			// The range box is authored facing left, so it mirrors with the
			// mob. Shifting it onto the mob's position turns it into a box in
			// world coordinates that the player is either inside or not.
			// A ball that reached the player has already done the hit test by
			// arriving. Measuring it against the swing's range box as well
			// would throw away every long shot, which is the entire point of
			// a ranged attack.
			if (by_projectile)
			{
				int32_t power = ae.magic ? ae.matk : ae.watk;

				return MobAttack(
					randomizer.next_int(static_cast<int32_t>(power * 0.8f), power),
					get_position(), id, oid);
			}

			Rectangle<int16_t> box = ae.range;

			if (!flip)
				box = Rectangle<int16_t>(
					static_cast<int16_t>(-ae.range.right()),
					static_cast<int16_t>(-ae.range.left()),
					ae.range.top(), ae.range.bottom());

			box.shift(get_position());

			if (!box.contains(target))
				return MobAttack();

			int32_t power = ae.magic ? ae.matk : ae.watk;
			int32_t damage = randomizer.next_int(
				static_cast<int32_t>(power * 0.8f), power);

			return MobAttack(damage, get_position(), id, oid);
		}

		return MobAttack();
	}

	void Mob::apply_death()
	{
		set_stance(Stance::DIE);
		diesound.play();
		dying = true;

		// A dead mob's shots die with it, and a swing it never finished must
		// not land afterwards.
		bullets.clear();
		attacking = false;
		pending_hit = false;
		pending_projectile = false;
	}

	bool Mob::is_alive() const
	{
		return active && !dying;
	}

	bool Mob::is_in_range(const Rectangle<int16_t>& range) const
	{
		if (!active)
			return false;

		Rectangle<int16_t> bounds = animations.at(stance).get_bounds();
		bounds.shift(get_position());

		return range.overlaps(bounds);
	}

	Point<int16_t> Mob::get_head_position() const
	{
		Point<int16_t> position = get_position();

		return get_head_position(position);
	}
}