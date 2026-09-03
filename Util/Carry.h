//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace ms
{
	// THE MEMORY CARD.
	//
	// Every handheld here can host a world of its own, and the same character
	// has to be playable on somebody else's and come home with whatever it did
	// there. That is how PSO worked: the character lived on the card in your
	// pocket, and the server was somewhere to play rather than somewhere to
	// live.
	//
	// Cosmic is not built that way - a character lives in the database of
	// whichever server it is on - so the card has to be kept up to date by
	// hand, out of the host's own database, through the carry port
	// (net.server.carry.CarryPort, one HTTP endpoint on 8585).
	//
	// WHY THE CARD IS FETCHED RATHER THAN WRITTEN BY THE SERVER: there is
	// nowhere on Android that both sides can meet. Cosmic writes inside
	// Termux's private storage, which the game cannot read; the game can only
	// write its own directory, which Termux cannot reach; and since Android 13
	// neither can use /sdcard/Download as a middle ground. So the game asks
	// for its card over a socket - which needs no permission of any kind - and
	// keeps it in the one place it owns.
	//
	// WHEN. While playing on this device's OWN server the card is refreshed on
	// a timer, so it is never more than a minute behind and a flat battery
	// costs a minute rather than an evening. Joining somebody else's world
	// sends the card ahead of logging in; leaving takes the updated one back.
	class Carry
	{
	public:
		static Carry& get()
		{
			static Carry instance;

			return instance;
		}

		// Where the card lives, inside the app's own folder - the only place
		// that survives the app closing and needs no permission.
		static std::string card_path();

		// Whether a card has ever been written here.
		bool has_card() const;

		// The account these characters belong to. Read out of the card, so it
		// survives a restart without the login screen having to remember.
		const std::string& account() const { return who; }

		void set_account(const std::string& name);

		// --- talking to a world ---------------------------------------------

		// Is a carry port answering there? Cheap, and the only way to know
		// whether a host can take a visitor before promising one it can.
		bool reachable(const std::string& host);

		// Fetch this account from `host` and write it over the card.
		// `host` is an address, or empty for this device's own server.
		bool fetch(const std::string& host);

		// Send the card to `host`, so it can be logged into there.
		bool deliver(const std::string& host);

		// --- the timer ------------------------------------------------------

		// Called every frame. Refreshes the card while playing at home, and
		// does nothing at all anywhere else.
		void update();

		// Playing on somebody else's world, so the card is theirs to update
		// and must not be overwritten from here.
		void set_visiting(bool away, const std::string& host);

		// COMING BACK FROM SOMEBODY ELSE'S WORLD.
		//
		// Fetches the card from the host that was just left, so whatever was
		// done as a guest comes home. Returns at once - the work happens on
		// its own thread, because this is called at the moment the connection
		// drops and a four-second pause there reads as a hung game.
		//
		// Deliberately AFTER the game connection has gone: Cosmic writes the
		// character out when a player disconnects, so fetching before that
		// would collect the last autosave and quietly lose up to a minute of
		// play. The carry port is a separate socket and is still answering.
		void come_home();

		// Whether a fetch is in flight, so nothing starts a second one.
		bool busy() const { return working.load(); }

		bool visiting() const { return away_from_home; }
		const std::string& host() const { return away_host; }

		// The last thing that went wrong, for a screen to show.
		const std::string& trouble() const { return last_trouble; }

	private:
		Carry() = default;

		// One request. Returns the body, and sets `ok` from the status line -
		// a 404 has a body too, and treating it as data is how a "character"
		// full of an error message would get written over a real card.
		std::string request(const std::string& host, const std::string& method,
			const std::string& path, const std::string& body, bool& ok);

		std::string who;
		std::string last_trouble;

		bool away_from_home = false;
		std::string away_host;

		// Frames until the next refresh. The server autosaves every 60
		// seconds, so anything much finer only copies the same rows again.
		int32_t until_refresh = 0;

		// EVERY FETCH RUNS OFF THE FRAME.
		//
		// A request waits up to four seconds for an answer, and a device that
		// has walked out of range uses all four. On the game thread that is a
		// four-second freeze during play - which is worse than a stale card.
		std::atomic<bool> working { false };

		// Guards the card file against a refresh and a come-home overlapping.
		std::mutex card_lock;
	};
}
