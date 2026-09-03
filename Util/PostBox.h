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
#include <vector>

namespace ms
{
	// WRITING TO SOMEBODY WHO IS NOT THERE.
	//
	// Two cases have to work and they pull in opposite directions:
	//
	//   * the couch, WHEN A THUNDERSTORM HAS TAKEN THE LIGHTS OUT. No
	//     internet, no router. Everything must still work.
	//   * somebody in ANOTHER STATE, who is not online now and will not be
	//     for days.
	//
	// The second is store-and-forward. A message written in a car with no
	// signal goes into an OUTBOX on this device; when a network next appears
	// it is handed on; the person it is for collects it whenever they next
	// connect. Neither end is ever waiting for the other to be awake, which
	// is the one thing a direct connection cannot arrange however clever it
	// is.
	//
	// ⚠ NOTHING HERE IS REQUIRED. With no relay set up, or no internet at
	// all, messages still reach anybody on the same world - the game server
	// itself keeps a post box (net.server.carry.Messages). The relay only
	// widens who can be reached, and its absence is never an error.
	//
	// ⚠ NO ITEMS. Text and mesos only. An item has to LEAVE one database and
	// ARRIVE in another, and a hand-off that half-fails either loses it or
	// makes two of it. Gifts are Duey's job, inside one world, where there is
	// one database and nothing to reconcile.
	class PostBox
	{
	public:
		static PostBox& get()
		{
			static PostBox instance;

			return instance;
		}

		struct Note
		{
			int64_t id = 0;
			std::string from;
			std::string body;
			int32_t mesos = 0;
			int64_t sent = 0;

			// Set on a note this device wrote and has not managed to hand on
			// yet - the ones sitting in the outbox waiting for a network.
			bool pending = false;

			// Who it is FOR. Only meaningful on a pending one.
			std::string to;
		};

		// Whose messages these are. Set at login, like the carry card.
		void set_account(const std::string& name);

		const std::string& account() const { return who; }

		// --- writing ---------------------------------------------------------

		// Queue one. It goes out at once if anything can be reached, and sits
		// in the outbox if not - which is the whole point.
		void send(const std::string& to, const std::string& body, int32_t mesos = 0);

		// --- reading ---------------------------------------------------------

		// Everything received, newest last. Read from this device's own file,
		// so it is there with no network at all.
		const std::vector<Note>& inbox() const { return received; }

		// Still waiting to go out.
		const std::vector<Note>& outbox() const { return pending; }

		// How many have arrived since the player last opened the page. This
		// is what the envelope's badge counts.
		int32_t unread() const { return unread_count; }

		void mark_all_read();

		// --- who you can write to --------------------------------------------

		// EVERYBODY THIS DEVICE HAS EVER SEEN.
		//
		// Typing a name on a handheld, with a stylus, getting the spelling
		// exactly right, into a field you cannot see - every one of those is
		// a way to fail silently, and the message just never arrives. So the
		// name is CHOSEN, never typed.
		//
		// Fed from two places: anybody standing on your map, and anybody who
		// has written to you. Both mean "somebody you actually play with".
		void remember(const std::string& raw);

		const std::vector<std::string>& known() const { return seen; }

		// --- the beat --------------------------------------------------------

		// Called every frame. Flushes the outbox and asks for anything new,
		// on a slow timer and never on the frame's own thread.
		void update();

		// Load what is on disk. Called once, when the account is known.
		void load();

		const std::string& trouble() const { return last_trouble; }

		// ANNOUNCED WHERE EVERY DEVICE CAN SEE IT.
		//
		// The Mail page lives on the second screen, which one handheld in
		// three actually has - so on the others a message arrived, was
		// written to disk, and told nobody. The chat log is on every screen.
		//
		// Filled on the worker thread and drained on the game thread, because
		// drawing is not something a worker may touch.
		std::vector<std::string> take_announcements();

	private:
		PostBox() = default;

		static std::string inbox_path();
		static std::string outbox_path();

		void save_inbox();
		void save_outbox();
		void save_known();
		static std::string known_path();

		// One exchange with a post box. `host` is empty for this device's own
		// world; otherwise an address or the relay's URL.
		bool deliver_to(const std::string& where, const Note& note);
		bool fetch_from(const std::string& where);

		// The relay, if one has been configured. Empty is normal and fine.
		static std::string relay();

		std::string who;
		std::string last_trouble;

		std::vector<Note> received;
		std::vector<Note> pending;

		// Names, oldest first, no duplicates. Kept on disk beside the post.
		std::vector<std::string> seen;

		int32_t unread_count = 0;

		std::vector<std::string> to_announce;

		// Every exchange runs off the frame - a device out of range takes the
		// full timeout, and that must never be a stutter during play.
		std::atomic<bool> working { false };
		std::mutex files;

		int32_t until_poll = 0;
		bool loaded = false;
	};
}
