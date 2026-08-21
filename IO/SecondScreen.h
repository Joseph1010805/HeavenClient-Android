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

#include "UIElement.h"

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

		// The panel is drawn in its own pixels, one to one.
		//
		// It was briefly laid out in a smaller space of the same shape, on the
		// grounds that one set of positions would then suit any second screen.
		// That was wrong: the pages are the game's own windows and those are
		// drawn at fixed pixel sizes - the world map's frame alone is 654x537 -
		// so a smaller space meant shrinking every one of them and losing the
		// sharpness. Pages are centred in whatever room there is instead, which
		// adapts to a different panel without touching the artwork.

		// Everything the panel shows. Draws in the space above.
		void draw();

		// A touch on the panel, in its own pixels - Java hands them over
		// separately because Android delivers them to the Presentation rather
		// than to SDL.
		void touch(float x, float y, bool down, bool up);

		// Where the last touch was, in panel pixels.
		Point<int16_t> cursor();

		// Ask the panel to show a window, and say whether it took it.
		//
		// The panel exists so these windows open DOWN HERE rather than over the
		// game, so when it hosts one, the key that opens it must turn the panel
		// to that page instead of building a second copy on the main screen.
		// Two copies of one window is not just untidy: there is one cursor and
		// one tooltip between the two screens, so both copies fight over them
		// and the pointer flicks from screen to screen as each is touched.
		//
		// False when there is no second screen, or the panel does not host that
		// window yet - the caller then opens it the ordinary way.
		bool show_window(UIElement::Type type);
	}
}
