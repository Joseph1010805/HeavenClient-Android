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
	// Playing with nothing to connect to.
	//
	// The answer is not an offline mode in this client - it is the SERVER
	// travelling with you. One device carries Cosmic, plays against its own
	// loopback, and anybody else joins over a phone hotspot. It is the real
	// server, so offline behaves exactly like home: same quests, same drops,
	// same rules. A faked offline mode would be second-rate forever.
	//
	// Cosmic cannot live inside this app - it wants a real JVM and a database
	// daemon - so it runs in Termux, and this asks Termux to start it. See
	// LocalServer.java for why it is an ASK and not a launch.
	//
	// Off Android none of this exists and every call is a no-op, because a
	// desktop already has whatever it needs.
	namespace LocalServer
	{
		// The two places this client can look for a server. HOME is the PC on
		// the LAN; HERE is this device, over loopback.
		//
		// Cosmic needs no second configuration for the difference:
		// `Server.getInetSocket` hands a loopback client its LOCALHOST address
		// and a LAN client its LANHOST one, by itself. So the machine running
		// the server still answers everybody else at the same time.
		constexpr char HERE[] = "127.0.0.1";

		// Whether the client is currently pointed at this device.
		bool is_offline();

		// Point it somewhere and remember the choice. Takes effect on the
		// next connection, so the caller should be at the login screen.
		void set_offline(bool offline);

		// The address used when NOT offline. Remembered separately, so
		// switching back does not need it typed in again.
		std::string home_address();
		void set_home_address(const std::string& address);

		// Whether the machinery to run a server here exists at all - Termux
		// installed. False on a device that has never been set up, which is
		// the honest thing to tell the player rather than failing later.
		bool can_host();

		// Everything that has to be true before hosting will work, so the
		// screen can show a list of ticks and crosses rather than failing
		// later with one vague message. Somebody setting a device up for the
		// first time should be able to SEE what is left to do.
		struct Readiness
		{
			bool termux = false;
			bool permission = false;
			// The server is ANSWERING, not merely installed - see the note in
			// LocalServer.java about why the question had to change.
			bool server = false;
			bool wifi_direct = false;

			// What has to be true before hosting can even be ATTEMPTED. The
			// server answering is a status, not a prerequisite: starting it
			// is the whole point of pressing HOST, so requiring it first
			// would mean it could never be started at all.
			//
			// Wi-Fi Direct only matters where there is no other network, so
			// it is reported and never required.
			bool can_try() const { return termux && permission; }
		};

		Readiness check();

		// Ask for the server to start. Returns whether the request was
		// accepted, NOT whether the server came up - that takes a minute and
		// shows itself when the client connects.
		bool start();
	}
}
