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
#include "MapReactors.h"
#include "Reactor.h"

namespace ms
{
	void MapReactors::draw(Layer::Id layer, double viewx, double viewy, float alpha) const
	{
		reactors.draw(layer, viewx, viewy, alpha);
	}

	// Spawns all reactors to map with proper footholds
	void MapReactors::update(const Physics& physics)
	{
		for (; !spawns.empty(); spawns.pop())
		{
			const ReactorSpawn& spawn = spawns.front();

			int32_t oid = spawn.get_oid();

			if (auto reactor = reactors.get(oid))
				reactor->makeactive();
			else
				reactors.add(spawn.instantiate(physics));
		}

		reactors.update(physics);
	}

	void MapReactors::trigger(int32_t oid, int8_t state)
	{
		if (Optional<Reactor> reactor = reactors.get(oid))
			reactor->set_state(state);
	}

	void MapReactors::spawn(ReactorSpawn&& spawn)
	{
		spawns.emplace(std::move(spawn));
	}

	void MapReactors::remove(int32_t oid, int8_t state, Point<int16_t> position)
	{
		if (Optional<Reactor> reactor = reactors.get(oid))
			reactor->destroy(state, position);
	}

	void MapReactors::clear()
	{
		reactors.clear();

		// The pending queue as well as the live objects.
		//
		// update() drains `spawns` into `reactors`, and a map change stops the
		// update loop (the graphics are locked and the timer restarted) while
		// the network thread keeps queueing. Anything that arrived during the
		// changeover therefore outlived the wipe and was instantiated into the
		// NEXT map - which is how four Tutorial Tinos from the Cygnus tutorial
		// ended up on Maple Road after a character change.
		std::queue<ReactorSpawn>().swap(spawns);
	}

	MapObjects* MapReactors::get_reactors()
	{
		return &reactors;
	}
}