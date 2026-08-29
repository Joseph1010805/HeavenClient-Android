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

#include <mutex>
#include <string>

namespace ms
{
	// TALKING TO THE GAME.
	//
	// These devices have no keyboard. The Thor and the RP5 have a d-pad and an
	// on-screen key grid; the Quest has a laser pointer and nothing else. Every
	// one of them has a microphone, and saying a sentence is far quicker than
	// spelling it out one letter at a time with a thumbstick.
	//
	// Recognition is ENTIRELY ON THE DEVICE - Vosk with a small English model
	// deployed beside the game data. No cloud service, nothing leaving the
	// house, and it works with the router unplugged, which is the same promise
	// the rest of this build makes.
	//
	// The model is NOT in the apk. It is ~40MB, it is not ours to redistribute
	// on a release page, and there is already a pipeline that puts large files
	// on a device - tools/deploy_data.sh. Absent, this simply reports itself
	// unavailable and the microphone buttons stay quiet rather than lying.
	class Speech
	{
	public:
		static Speech& get();

		// Whether there is a recogniser to talk to at all. False on desktop,
		// and false on a device where the model was never deployed.
		bool available() const;

		// Begins listening. False if the recogniser could not be started -
		// no model, no microphone permission, or already running.
		bool start();

		// Stops early. Whatever had been recognised up to that point is still
		// delivered, because a sentence cut short is usually still the thing
		// the player meant to say.
		void stop();

		bool is_listening() const;

		// Takes the last finished phrase and clears it. Empty when nothing has
		// been recognised since the last call.
		//
		// PULLED, never pushed. Recognition finishes on Vosk's own thread, and
		// a Textfield is not something to touch from there - the UI collects
		// this in its update(), on the thread that owns it.
		std::string take_phrase();

		// The sentence SO FAR, as the recogniser currently believes it.
		//
		// PEEKED, not taken - it is read every frame to fill the bubble over
		// the player's head, and clearing it on read would make the bubble
		// flicker empty between updates.
		//
		// Partials rewrite themselves as the recogniser changes its mind, which
		// is why they are kept out of the chat BOX. Over a character's head
		// that is not a defect - it reads as somebody thinking aloud.
		std::string peek_partial() const;

		// Wipes the partial. Called when a sentence has been acted on, so the
		// next one starts from nothing.
		void clear_partial();

		// Called from the Java side, on the recogniser's thread.
		void deliver(const std::string& text);
		void deliver_partial(const std::string& text);
		void set_listening(bool value);

	private:
		Speech();

		Speech(const Speech&) = delete;
		Speech& operator=(const Speech&) = delete;

		mutable std::mutex lock;

		std::string phrase;
		std::string partial;
		bool listening;
	};
}
