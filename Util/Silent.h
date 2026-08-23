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

#include <string>

namespace ms
{
	// Says so when the client does nothing.
	//
	// The expensive bugs in this client are not the ones that crash. They are
	// the ones where a tap reaches the bottom of a switch with no matching
	// case, a packet arrives with no handler, or an item is drawn from art
	// that was never found - and the game carries on, silently, doing
	// nothing. From the outside that is indistinguishable from a feature
	// nobody built, and it stays hidden until somebody at the table happens
	// to try that exact thing.
	//
	// Four such bugs shipped in one morning: every mask in the game invisible
	// when worn, the whole cash tab of the bag inert, cash equips knocking
	// real gear off, and leaving the shop disconnecting the player. Not one
	// printed a single character.
	//
	// The cause is not that they are hard to see. It is that C++ says nothing
	// when it does nothing, and every fall-through in this codebase inherited
	// that default. This inverts it: silence becomes a line of output, so one
	// ordinary session leaves a list of everything that quietly did not work.
	//
	// Use it at the BOTTOM of dispatch - the default case, the unmatched
	// lookup, the guard that returns early - and not for conditions that are
	// ordinary. Reports are de-duplicated by text, so a tap repeated fifty
	// times prints once.
	//
	//     adb logcat -s HeavenSilent:I
	//
	namespace Silent
	{
		// `where` is the call site - "UIItemInventory::activate_slot".
		// `what` says what was asked for and went unanswered, with enough
		// numbers in it to act on: "tab=5 item=5211045 - no case for tab".
		void report(const char* where, const std::string& what);

		// How many distinct reports have been made. Somewhere to hang a
		// summary later.
		size_t count();
	}
}
