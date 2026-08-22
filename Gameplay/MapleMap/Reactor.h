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

#include "../Audio/Audio.h"
#include "../Graphics/Animation.h"

#include "../Gameplay/Physics/Physics.h"

#include <vector>
#include <map>

namespace ms
{
	class Reactor : public MapObject
	{
	public:
		Reactor(int32_t oid, int32_t rid, int8_t state, Point<int16_t> position);

		void draw(double viewx, double viewy, float alpha) const override;
		int8_t update(const Physics& physics);

		void set_state(int8_t state);
		void destroy(int8_t state, Point<int16_t> position);

		bool is_hittable() const;

	private:
		// Plays the noise a given state makes when it is struck, if the data
		// has one for it.
		void play_state_sound(int8_t which);

	public:

		// Check if this mob collides with the specified rectangle.
		bool is_in_range(const Rectangle<int16_t>& range) const;

	private:
		int32_t oid;
		int32_t rid;
		int8_t state;
		//int8_t stance; // TODO: ??
		// TODO: These are in the GMS client
		//bool movable; // TODO: Snowball?
		//int32_t questid;
		//bool activates_by_touch;

		nl::node src;
		std::map<int8_t, Animation> animations;
		bool animation_ended;

		// `active` deliberately does NOT live here - it belongs to MapObject,
		// which sets it true on construction and clears it in deactivate().
		// Re-declaring it shadowed the base with an uninitialised copy, so
		// is_in_range read whatever happened to be in that memory: the box
		// still drew, because drawing asks the base class, but attacking it
		// consulted the garbage. Stable per object and different per box,
		// which is exactly what "the boxes only break sometimes" looked like.
		bool hittable;
		bool dead;

		Animation normal;

		// One sound per state, which is how the data stores them:
		// Sound.img/Reactor.img/<plain id>/<state>/Hit. There is no separate
		// break sound - breaking is simply the last state being hit.
		std::map<int8_t, Sound> statesounds;
	};
}