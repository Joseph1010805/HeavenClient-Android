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
#include "MapMobs.h"
#include "Mob.h"

#include "../../Constants.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIStatusbar.h"

#include <algorithm>
#include <iostream>
#include <map>

namespace ms
{
	void MapMobs::draw(Layer::Id layer, double viewx, double viewy, float alpha) const
	{
		mobs.draw(layer, viewx, viewy, alpha);
	}

	void MapMobs::update(const Physics& physics)
	{
		for (; !spawns.empty(); spawns.pop())
		{
			const MobSpawn& spawn = spawns.front();

			// ⚠ A SPAWN CANCELS A KILL THAT HAS NOT LANDED YET.
			//
			// The server REFRESHES every monster when a map transition
			// finishes - PlayerMapTransitionHandler sends destroy, then spawn,
			// then hands control over. Arriving in that order the pair means
			// "replace this", and applying both immediately came out right.
			//
			// The death hold below delays a kill by DEATH_HOLD so the killing
			// blow is drawn before the monster falls. That reordered this
			// pair: the spawn was applied at once and the destroy landed a
			// fifth of a second later, on top of the monster that had just
			// replaced it. Every mob on the map appeared for that fifth of a
			// second, played its death sound and went inactive - invisible,
			// motionless and impossible to touch, while this client went on
			// holding all four of them.
			//
			// A delay may reorder events that were only ever correct in the
			// order they arrived. This is the guard for that.
			for (size_t i = 0; i < pending.size(); )
			{
				if (pending[i].oid == spawn.get_oid())
					pending.erase(pending.begin() + i);
				else
					i++;
			}

			if (Optional<Mob> mob = mobs.get(spawn.get_oid()))
			{
				int8_t mode = spawn.get_mode();

				if (mode > 0)
					mob->set_control(mode);

				mob->makeactive();
			}
			else
			{
				mobs.add(spawn.instantiate());
			}
		}

		// Before the mobs themselves, so a death that comes due this frame is
		// animated from this frame rather than the next.
		update_pending();

		mobs.update(physics);
	}

	void MapMobs::spawn(MobSpawn&& spawn)
	{
		spawns.emplace(std::move(spawn));
	}

	void MapMobs::remove(int32_t oid, int8_t animation)
	{
		// ⚠ THE DEATH WAITS FOR THE BLOW THAT CAUSED IT.
		//
		// A hit is drawn LATE on purpose - Combat queues its damage effect
		// behind Char::get_attackdelay, so the number and the flinch land on
		// the frame the weapon actually connects rather than the frame the
		// button was pressed. The kill had no such delay: it arrives from the
		// server and was applied the instant it was read.
		//
		// So a killing blow played backwards. The monster started dying, and
		// only afterwards did the strike that killed it appear.
		//
		// Held for a beat instead. Not matched to the exact attack delay,
		// which the mob cannot know - the attacker, the weapon and the attack
		// speed all move it - but long enough to cover the usual range and
		// short enough that nothing feels sticky.
		pending.push_back({ oid, animation, DEATH_HOLD });
	}

	void MapMobs::update_pending()
	{
		for (size_t i = 0; i < pending.size(); )
		{
			Pending& p = pending[i];

			p.left = static_cast<int16_t>(p.left - Constants::TIMESTEP);

			if (p.left > 0)
			{
				i++;
				continue;
			}

			if (Optional<Mob> mob = mobs.get(p.oid))
				mob->kill(p.animation);

			pending.erase(pending.begin() + i);
		}
	}

	void MapMobs::clear()
	{
		mobs.clear();

		// Deaths that never came due. A map change removes the monster they
		// referred to, and holding an oid across a map is how a fresh monster
		// with a recycled id gets killed on arrival.
		pending.clear();

		// The pending queue as well as the live objects.
		//
		// update() drains `spawns` into `mobs`, and a map change stops the
		// update loop (the graphics are locked and the timer restarted) while
		// the network thread keeps queueing. Anything that arrived during the
		// changeover therefore outlived the wipe and was instantiated into the
		// NEXT map - which is how four Tutorial Tinos from the Cygnus tutorial
		// ended up on Maple Road after a character change.
		std::queue<MobSpawn>().swap(spawns);
	}

	void MapMobs::set_control(int32_t oid, bool control)
	{
		int8_t mode = control ? 1 : 0;

		if (Optional<Mob> mob = mobs.get(oid))
			mob->set_control(mode);
	}

	void MapMobs::grant_skill(int32_t oid, int8_t skill_id, int8_t skill_level)
	{
		if (Optional<Mob> mob = mobs.get(oid))
			mob->grant_skill(skill_id, skill_level);
	}

	void MapMobs::send_mobhp(int32_t oid, int8_t percent, uint16_t playerlevel)
	{
		if (Optional<Mob> mob = mobs.get(oid))
		{
			mob->show_hp(percent, playerlevel);

			// A boss also drives the wide gauge at the top of the screen.
			if (mob->is_boss())
				if (auto statusbar = UI::get().get_element<UIStatusbar>())
					statusbar->update_boss_hp(mob->get_name(), percent,
						mob->get_hp_tag_color(), mob->get_hp_tag_bgcolor());
		}
	}

	void MapMobs::send_movement(int32_t oid, Point<int16_t> start, std::vector<Movement>&& movements)
	{
		if (Optional<Mob> mob = mobs.get(oid))
			mob->send_movement(start, std::move(movements));
	}

	void MapMobs::send_attack(AttackResult& result, const Attack& attack, const std::vector<int32_t>& targets, uint8_t mobcount)
	{
		for (auto& target : targets)
		{
			if (Optional<Mob> mob = mobs.get(target))
			{
				result.damagelines[target] = mob->calculate_damage(attack);
				result.mobcount++;

				if (result.mobcount == 1)
					result.first_oid = target;

				if (result.mobcount == mobcount)
					result.last_oid = target;
			}
		}
	}

	void MapMobs::apply_damage(int32_t oid, int32_t damage, bool toleft, const AttackUser& user, const SpecialMove& move)
	{
		if (Optional<Mob> mob = mobs.get(oid))
		{
			mob->apply_damage(damage, toleft);

			// TODO: Maybe move this into the method above too?
			move.apply_hiteffects(user, *mob);
		}
	}

	bool MapMobs::contains(int32_t oid) const
	{
		return mobs.contains(oid);
	}

	size_t MapMobs::count() const
	{
		return mobs.size();
	}

	int32_t MapMobs::find_colliding(const MovingObject& moveobj) const
	{
		Range<int16_t> horizontal = Range<int16_t>(moveobj.get_last_x(), moveobj.get_x());
		Range<int16_t> vertical = Range<int16_t>(moveobj.get_last_y(), moveobj.get_y());

		Rectangle<int16_t> player_rect = {
			horizontal.smaller(),
			horizontal.greater(),
			vertical.smaller() - 50,
			vertical.greater()
		};

		auto iter = std::find_if(
			mobs.begin(),
			mobs.end(),
			[&player_rect](auto& mmo)
			{
				Optional<Mob> mob = mmo.second.get();
				return mob && mob->is_alive() && mob->is_in_range(player_rect);
			}
		);

		if (iter == mobs.end())
			return 0;

		return iter->second->get_oid();
	}

	void MapMobs::set_target(Point<int16_t> position)
	{
		for (auto& entry : mobs)
			if (auto* mob = static_cast<Mob*>(entry.second.get()))
				mob->set_target(position);
	}

	std::vector<std::pair<int8_t, MobAttack>> MapMobs::take_landed_attacks(Point<int16_t> target)
	{
		std::vector<std::pair<int8_t, MobAttack>> landed;

		for (auto& entry : mobs)
		{
			auto* mob = static_cast<Mob*>(entry.second.get());

			if (mob == nullptr || !mob->has_pending_hit())
				continue;

			int8_t index = mob->pending_attack_index();

			if (MobAttack attack = mob->take_pending_hit(target))
				landed.emplace_back(index, attack);
		}

		return landed;
	}

	MobAttack MapMobs::create_attack(int32_t oid) const
	{
		if (Optional<const Mob> mob = mobs.get(oid))
			return mob->create_touch_attack();
		else
			return {};
	}

	Point<int16_t> MapMobs::get_mob_position(int32_t oid) const
	{
		if (auto mob = mobs.get(oid))
			return mob->get_position();
		else
			return Point<int16_t>(0, 0);
	}

	Point<int16_t> MapMobs::get_mob_head_position(int32_t oid) const
	{
		if (Optional<const Mob> mob = mobs.get(oid))
			return mob->get_head_position();
		else
			return Point<int16_t>(0, 0);
	}

	MapObjects* MapMobs::get_mobs()
	{
		return &mobs;
	}
}