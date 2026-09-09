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

#include "Footholdtree.h"

namespace ms
{
	// Class that uses physics engines and the collection of platforms to determine object movement.
	class Physics
	{
	public:
		// Downward acceleration, in pixels per step per step. Nexon's 2000
		// px/s^2 over 125 steps/s: 2000 / 125^2. Was 0.14, about 9% heavy.
		//
		// Public because anything that LAUNCHES something needs it. A launch
		// speed on its own says nothing about how far the thing will actually
		// travel - only `speed^2 / (2 * GRAVFORCE)` does - so a caller that
		// wants to aim for a height has to be able to see this number rather
		// than keep its own copy of it and drift.
		static constexpr double GRAVFORCE = 0.128;

		Physics(nl::node src);
		Physics();

		// Move the specified object over the specified game-time.
		void move_object(PhysicsObject& tomove) const;
		// Determine the point on the ground below the specified position.
		Point<int16_t> get_y_below(Point<int16_t> position) const;
		// Return a reference to the collection of platforms.
		const Footholdtree& get_fht() const;

	private:
		void move_normal(PhysicsObject&) const;
		void move_flying(PhysicsObject&) const;
		void move_swimming(PhysicsObject&) const;

		Footholdtree fht;
	};
}