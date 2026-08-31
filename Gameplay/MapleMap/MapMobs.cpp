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

		mobs.update(physics);
	}

	void MapMobs::spawn(MobSpawn&& spawn)
	{
		spawns.emplace(std::move(spawn));
	}

	void MapMobs::remove(int32_t oid, int8_t animation)
	{
		if (Optional<Mob> mob = mobs.get(oid))
			mob->kill(animation);
	}

	void MapMobs::clear()
	{
		mobs.clear();

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