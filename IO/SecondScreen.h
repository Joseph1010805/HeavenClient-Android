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

		// The panel's copy of a window, if it hosts one.
		//
		// The rest of the client finds its windows through UI::get_element,
		// and a window living on the panel is not in UI's list - so every
		// notification the server sends about, say, the inventory went to a
		// window that may not even exist while the panel's copy went on showing
		// what it had. This is how those find their way here.
		UIElement* hosted(UIElement::Type type);

		// Draw the panel's hover information on the MAIN screen. Called from
		// the main screen's own pass, so it lands in that screen's pixels.
		// Scroll whatever page the panel is showing. Driven by the LEFT stick -
		// the top screen is the left one to reach for, and walking moved to
		// the d-pad to free it.
		void scroll(double yoffset);

		void draw_top_tooltip();

		// What the page on the panel currently has picked out, as something a
		// key could be bound to - NONE if nothing is, or there is no panel.
		//
		// The panel selects where the main screen drags, so an item chosen
		// down here cannot be carried up to the quickslot bar. The bar reaches
		// down instead.
		Keyboard::Mapping selected_mapping();

		// The last thing picked out on ANY page, which survives a page turn.
		// What the hotkey page binds.
		Keyboard::Mapping carried_mapping();

		// Called once whatever was being carried has been put down.
		void clear_carried();

		// Turn the panel to the hotkey page. Does nothing without a panel.
		void show_hotkeys();

		// Turn the panel to the trade page and hand back its window, BUILDING
		// it if it has not been opened yet.
		//
		// Not the same as hosted(TRADE): pages are built the first time they
		// are looked at, and the first thing that happens in a trade is a
		// packet arriving - before anybody has looked at anything. Asking for
		// the window without building it would drop the opening move.
		//
		// Null where there is no panel, and the caller then puts the window
		// on the main screen instead.
		UIElement* open_trade();

		// The same for the bank, which arrives the same way: unasked for,
		// because an NPC opened it. See open_trade.
		UIElement* open_storage();

		// Put a shop on the panel instead of over the game.
		//
		// Does nothing where there is no second screen - available() is a
		// CAPABILITY check, not a device one, so the RP5 and the Quest simply
		// keep the shop on their only screen and no model names appear
		// anywhere. Returns whether the panel took it.
		bool show_shop(UIElement* shop, bool equipment);

		// Whether the pointer is currently on the panel. A window opened while
		// it is gets put somewhere the player is already looking.
		bool has_cursor();

		// Play the level-up flourish on the panel. Does nothing where there is
		// no panel, so the one-screen build is unaffected.
		void play_levelup();

		// THE SAME PANEL ON A DEVICE WITH ONLY ONE SCREEN.
		//
		// Everything above draws to a surface the RP5 and the Quest do not
		// have, so on those the panel did not exist at all - and with it went
		// Mail, Party, the bag pages, the stat popup and every menu. Half the
		// client was reachable only on the Thor.
		//
		// The panel never needed a second screen to work: every one of its
		// draw calls already takes the space to lay out in, and its touches
		// are given a position and that same space. So on a one-screen device
		// it is drawn OVER the game instead, from the main screen's own pass,
		// and put away again. Same panel, same pages, same code.
		//
		// It is deliberately a takeover rather than a corner widget: this is
		// what replaces the game's own icon row, and a menu that half-covers
		// the map is easier to read than one squeezed beside it.

		// True where there is no second display - the case this exists for.
		bool overlay_supported();

		// Whether it is currently over the game.
		bool overlay_showing();

		// Open or close it. Does nothing on a device that has a real panel,
		// which shows the same pages downstairs already.
		void toggle_overlay();

		// THE PANEL'S TOP-LEVEL SECTIONS, named so the status bar can ask for
		// one without this header having to expose the whole page list.
		//
		// These stand in for the game's own bottom-row menus on a one-screen
		// device: pressing Character down there opens OUR character section
		// rather than their icon popup.
		enum class Section
		{
			HOME,
			CHARACTER,
			ADVENTURE,
			SOCIAL,
			SETTINGS,
			DAILY
		};

		// Open the overlay at a section. Returns false where there is no
		// overlay - a device with a real panel - and the caller then does
		// whatever it did before.
		bool open_overlay(Section section);

		// Whether anything inside the panel is asking to be looked at, for the
		// single MENU icon that stands in for the whole row on a one-screen
		// device. False where there is no overlay - the Thor badges its own
		// buttons downstairs.
		bool overlay_alert();

		// Draw it, in the MAIN screen's pass and its design pixels. Nothing
		// happens unless it is open.
		void draw_overlay();

		// Offer a press to the overlay. Returns whether it took it - if it
		// did, the game must not also act on it, or a tap on a menu button
		// would walk the character as well.
		bool overlay_send_cursor(Point<int16_t> position, bool pressed, bool released);

	}
}
