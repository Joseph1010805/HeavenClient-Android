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

#include "../Template/Point.h"

namespace ms
{
	// The second display, where a handheld has one.
	//
	// SDL owns a single window on Android, so the other screen is a Surface
	// handed down from Java and drawn to through EGL directly. It uses the
	// SAME GL context as the main screen rather than a shared second one, so
	// every texture already uploaded - the sprite atlas above all - is usable
	// on both without being duplicated. A second context would have cost
	// another atlas.
	//
	// Everything here is a no-op when there is no second display, which is
	// what keeps the client identical on an ordinary phone.
	namespace SecondScreen
	{
		// Whether there is a surface to draw on. False on one-screen devices,
		// and while the surface is being created or has gone away.
		bool available();

		// The size of that surface in pixels.
		Point<int16_t> size();

		// Make the second screen current and clear it. Returns false if there
		// is nothing to draw to, in which case do not draw.
		bool begin();

		// Show what was drawn and put the main screen back. Must be paired
		// with a begin() that returned true.
		void end();
	}
}
