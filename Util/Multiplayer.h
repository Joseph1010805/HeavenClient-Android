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
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ms
{
	// Playing together, without anyone typing an address.
	//
	// There is no such thing as "online" and "offline" here. There is only
	// who is HOSTING. Every device carries a server; one of them runs it and
	// the others join. Playing alone is hosting with nobody joining - the
	// same thing, which is why the old HOME / THIS DEVICE switch was the
	// wrong shape.
	//
	// Three separate problems, kept separate on purpose:
	//
	//   1. Is there a network at all?    WifiDirect.java, and only needed
	//                                    where there is no router or hotspot
	//   2. Who is hosting on it?         Discovery.java - mDNS, works over
	//                                    ANY network including a Wi-Fi
	//                                    Direct group
	//   3. Is a server running here?     LocalServer.h - Termux and Cosmic
	//
	// Solving 2 is what removes addresses from the player's life, and it
	// works today over a phone's hotspot with none of 1.
	namespace Multiplayer
	{
		// A game somebody is hosting, as it should appear in a list: a name
		// a child recognises, the address that is nobody's business, and the
		// scrambled form of the six-digit code needed to get in.
		struct Game
		{
			std::string name;
			std::string address;

			// Zero means the host set no code - an older build, or one
			// started before codes existed. Joining is then open, as it was.
			uint32_t code = 0;
		};

		// WHAT THE SIX-DIGIT CODE IS AND IS NOT.
		//
		// It is the thing that makes joining DELIBERATE. Three handhelds on
		// one house wifi all see each other, and "which of these two identical
		// entries is Dad's" is a question a child should not have to answer by
		// trial and error. The host says a number out loud and that settles
		// it.
		//
		// It is NOT a password, and nothing here pretends otherwise. The
		// scrambled form travels in the announcement where anybody already on
		// the network can see it, and six digits is a million guesses, which
		// is no work at all for a computer. Anyone who can read the
		// announcement is already inside the house and on the wifi; the code
		// stops mistakes, not intruders. Do not build anything on it that
		// needs to be secret.
		uint32_t code_hash(const std::string& six_digits);

		// Announce that a game is running here, under this device's name, and
		// with the scrambled form of its code so joiners can check what they
		// type without the number itself being announced in the clear.
		bool start_hosting(const std::string& name, uint32_t code);
		void stop_hosting();

		// Start and stop listening for games. Browsing costs battery, so it
		// runs only while the JOIN list is on screen.
		bool start_browsing();
		void stop_browsing();

		// What has been found so far. Polled once a frame - the answer
		// changes on Android's own threads.
		std::vector<Game> games();

		// What to call this device by default.
		std::string suggested_name();

		// Whether there is a network to search at all - wifi or ethernet, not
		// mobile data, which reaches the internet and not the handheld next
		// to you. When this is false, Wi-Fi Direct is the only way anybody
		// plays together.
		bool on_network();

		// --- making a network where there is none ---

		bool wifi_direct_supported();

		// Whether the wifi RADIO is on, which is not the same as being
		// connected to anything. Wi-Fi Direct needs no network, no router and
		// no internet - but it does need the radio. Switching wifi off
		// switches the peer-to-peer side off with it, and every call comes
		// back BUSY, which reads exactly like a device that cannot do it.
		bool wifi_radio_on();

		// Put Android's own wifi switch in front of the player. An app has
		// not been allowed to turn wifi on by itself since Android 10.
		void open_wifi_settings();

		// Become the network. This device turns into a small access point and
		// always takes 192.168.49.1, which is why a client that has joined a
		// group knows where the server is even if discovery fails.
		bool create_group();
		void remove_group();

		// Look for a group to join. Android shows its own picker, and the
		// other device has to agree - joining somebody's network is their
		// decision too.
		bool find_groups();

		constexpr char GROUP_OWNER[] = "192.168.49.1";

		// What this device currently IS: nothing, the group owner, or a
		// client in somebody else's group.
		//
		// A device that OWNS a group is being a network, and cannot join
		// another one. That has to be undone before looking for a host.
		enum class Role : uint8_t { NONE, OWNER, CLIENT };

		Role role();
		void refresh_role();
	}
}
