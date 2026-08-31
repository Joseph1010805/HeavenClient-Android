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

namespace ms
{
	// PUSH-TO-TALK VOICE, ON THE LOCAL NETWORK.
	//
	// A thin front for VoiceChat.java, which does all of it: capture,
	// playback and a UDP broadcast to whoever else on the wifi is listening.
	// Nothing is encoded and nothing is routed - 16 kHz mono samples go out to
	// the subnet's broadcast address and everyone hears them. On a house wifi
	// that is cheap, and it means there is no roster to keep in step with who
	// is actually playing.
	//
	// A FRIEND ACROSS THE WORLD IS THE NEXT SLICE, not this one. It needs a
	// small relay running beside the Cosmic server, and it plugs into exactly
	// one method in the Java - VoiceChat.target(). Nothing here changes.
	//
	// THE MICROPHONE IS NOT SHARED. Vosk holds it whenever speech-to-text is
	// listening and Android will not reliably hand out a second recorder, so
	// start() refuses rather than opening one that silently reads nothing.
	//
	// Off on desktop, where none of the Java exists. Every call is safe there
	// and reports itself unavailable, so the button can be hidden instead of
	// lying about what it does.
	class Voice
	{
	public:
		static Voice& get();

		// Opens the socket and begins listening for other people. Does NOT
		// open the microphone - that waits for the button.
		bool start();
		void stop();

		// Whether the socket is open. Not whether anyone is talking.
		bool is_open() const;

		// HOLD AND RELEASE.
		//
		// Deliberately not a latch. These are handhelds with their speakers
		// on, in one room; an open microphone means every device rebroadcasts
		// every other device's output and the game's music with it.
		void set_talking(bool on);
		bool is_talking() const;

	private:
		Voice() = default;

		bool open = false;
		bool talking = false;
	};
}
