//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "PostBox.h"

#include "Silent.h"
#include "Http.h"

#include "../Configuration.h"
#include "../Gameplay/Stage.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>

namespace ms
{
	namespace
	{
		// Slower than the carry card's minute: a message is not urgent, and
		// every poll on a handheld in a pocket costs battery.
		constexpr int32_t POLL_FRAMES = 60 * 120;

		// The wire format, shared with the carry port and the relay:
		//
		//   MSG <id> <from> <mesos> <sent> <length>
		//   <length bytes of body>
		//
		// The body is COUNTED and comes last, so a message may contain tabs,
		// newlines, or anything else somebody types.
		std::vector<PostBox::Note> parse(const std::string& text)
		{
			std::vector<PostBox::Note> out;
			size_t at = 0;

			while (at < text.size())
			{
				size_t eol = text.find('\n', at);

				if (eol == std::string::npos)
					break;

				std::string head = text.substr(at, eol - at);
				at = eol + 1;

				if (head.rfind("MSG\t", 0) != 0)
					continue;

				std::vector<std::string> bits;
				size_t from = 4;

				while (true)
				{
					size_t tab = head.find('\t', from);

					bits.push_back(head.substr(from,
						tab == std::string::npos ? std::string::npos : tab - from));

					if (tab == std::string::npos)
						break;

					from = tab + 1;
				}

				if (bits.size() < 5)
					continue;

				PostBox::Note note;

				try
				{
					note.id = std::stoll(bits[0]);
					note.from = bits[1];
					note.mesos = std::stoi(bits[2]);
					note.sent = std::stoll(bits[3]);

					size_t length = static_cast<size_t>(std::stoul(bits[4]));

					if (at + length > text.size())
						break;

					note.body = text.substr(at, length);
					at += length + 1;
				}
				catch (...)
				{
					continue;
				}

				out.push_back(std::move(note));
			}

			return out;
		}

		// One record on disk. The body is counted here too, for the same
		// reason - a message with a newline in it must not become two.
		std::string as_line(const PostBox::Note& note)
		{
			std::ostringstream out;

			out << note.id << '\t' << note.from << '\t' << note.to << '\t'
				<< note.mesos << '\t' << note.sent << '\t'
				<< note.body.size() << '\n' << note.body << '\n';

			return out.str();
		}

		std::vector<PostBox::Note> read_file(const std::string& path)
		{
			std::ifstream in(path, std::ios::binary);

			if (!in)
				return {};

			std::stringstream buffer;
			buffer << in.rdbuf();

			std::string text = buffer.str();
			std::vector<PostBox::Note> out;
			size_t at = 0;

			while (at < text.size())
			{
				size_t eol = text.find('\n', at);

				if (eol == std::string::npos)
					break;

				std::string head = text.substr(at, eol - at);
				at = eol + 1;

				std::vector<std::string> bits;
				size_t from = 0;

				while (true)
				{
					size_t tab = head.find('\t', from);

					bits.push_back(head.substr(from,
						tab == std::string::npos ? std::string::npos : tab - from));

					if (tab == std::string::npos)
						break;

					from = tab + 1;
				}

				if (bits.size() < 6)
					break;

				PostBox::Note note;

				try
				{
					note.id = std::stoll(bits[0]);
					note.from = bits[1];
					note.to = bits[2];
					note.mesos = std::stoi(bits[3]);
					note.sent = std::stoll(bits[4]);

					size_t length = static_cast<size_t>(std::stoul(bits[5]));

					if (at + length > text.size())
						break;

					note.body = text.substr(at, length);
					at += length + 1;
				}
				catch (...)
				{
					break;
				}

				out.push_back(std::move(note));
			}

			return out;
		}

		// WHICH STACK AN ADDRESS NEEDS.
		//
		// Everything on the LAN is plain http and goes through asio. The
		// relay is https, and this build has no TLS - that leg goes through
		// Android's own stack instead. One place decides, so no caller has
		// to remember.
		bool is_far(const std::string& url)
		{
			return url.compare(0, 8, "https://") == 0;
		}

		Http::Reply fetch(const std::string& url)
		{
			return is_far(url) ? Http::secure_get(url) : Http::get(url);
		}

		Http::Reply deliver(const std::string& url, const std::string& body)
		{
			return is_far(url)
				? Http::secure_post(url, body)
				: Http::post(url, body);
		}

		void write_file(const std::string& path,
			const std::vector<PostBox::Note>& notes)
		{
			// BESIDE, THEN OVER. The moment this is most likely to be
			// interrupted - the game closing, a flat battery - is exactly
			// when it is being written, and a half-written inbox is somebody
			// losing their post.
			std::string temp = path + ".new";

			{
				std::ofstream out(temp, std::ios::binary | std::ios::trunc);

				if (!out)
					return;

				for (const PostBox::Note& note : notes)
					out << as_line(note);

				if (!out)
					return;
			}

			std::remove(path.c_str());
			std::rename(temp.c_str(), path.c_str());
		}
	}

	std::string PostBox::inbox_path()
	{
		return "/sdcard/Android/data/org.heavenclient.android/files/"
			"HeavenClient/messages_in.txt";
	}

	std::string PostBox::outbox_path()
	{
		return "/sdcard/Android/data/org.heavenclient.android/files/"
			"HeavenClient/messages_out.txt";
	}

	std::string PostBox::known_path()
	{
		return "/sdcard/Android/data/org.heavenclient.android/files/"
			"HeavenClient/known_players.txt";
	}

	namespace
	{
		// A NAME, WITH THE WIRE SCRAPED OFF IT.
		//
		// Notes arrive newline-separated, so a sender parsed out of one keeps
		// a trailing carriage return - and "ianjuicce" from the map and
		// "ianjuicce\r" from a message are two different strings that DRAW
		// IDENTICALLY. That is the duplicate the screen could not show: the
		// list was right, both entries looked the same, and save_known then
		// wrote the stray byte back to disk.
		std::string tidy(const std::string& name)
		{
			size_t first = name.find_first_not_of(" \t\r\n");

			if (first == std::string::npos)
				return std::string();

			size_t last = name.find_last_not_of(" \t\r\n");

			return name.substr(first, last - first + 1);
		}
	}

	void PostBox::remember(const std::string& raw)
	{
		std::string name = tidy(raw);

		if (name.empty() || name == who)
			return;

		std::lock_guard<std::mutex> hold(files);

		for (const std::string& had : seen)
			if (had == name)
				return;

		seen.push_back(name);

		save_known();
	}

	void PostBox::save_known()
	{
		std::string temp = known_path() + ".new";

		{
			std::ofstream out(temp, std::ios::binary | std::ios::trunc);

			if (!out)
				return;

			for (const std::string& name : seen)
				out << name << '\n';

			if (!out)
				return;
		}

		std::remove(known_path().c_str());
		std::rename(temp.c_str(), known_path().c_str());
	}

	std::string PostBox::relay()
	{
		// Absent is the NORMAL case and never an error - see Messages.h.
		return Setting<RelayURL>::get().load();
	}

	void PostBox::set_account(const std::string& name)
	{
		if (who == name)
			return;

		who = name;
		loaded = false;

		load();
	}

	void PostBox::load()
	{
		if (loaded || who.empty())
			return;

		std::lock_guard<std::mutex> hold(files);

		received = read_file(inbox_path());
		pending = read_file(outbox_path());

		{
			std::ifstream in(known_path());
			std::string line;

			while (std::getline(in, line))
			{
				while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
					line.pop_back();

				if (!line.empty())
					seen.push_back(line);
			}
		}

		for (Note& note : pending)
			note.pending = true;

		loaded = true;

		// Ask again shortly rather than waiting out a full cycle: somebody
		// who has just logged in wants to know what is waiting.
		until_poll = 60;
	}

	void PostBox::save_inbox()
	{
		write_file(inbox_path(), received);
	}

	void PostBox::save_outbox()
	{
		write_file(outbox_path(), pending);
	}

	std::vector<std::string> PostBox::take_announcements()
	{
		std::lock_guard<std::mutex> hold(files);

		std::vector<std::string> out;

		out.swap(to_announce);

		return out;
	}

	void PostBox::mark_all_read()
	{
		unread_count = 0;
	}

	void PostBox::send(const std::string& to, const std::string& body, int32_t mesos)
	{
		if (who.empty() || to.empty() || body.empty())
			return;

		Note note;
		note.id = 0;
		note.from = who;
		note.to = to;
		note.body = body;
		note.mesos = mesos;
		note.sent = 0;
		note.pending = true;

		{
			std::lock_guard<std::mutex> hold(files);

			pending.push_back(note);
			save_outbox();
		}

		// QUEUED FIRST, SENT SECOND, ALWAYS IN THAT ORDER.
		//
		// It is on this device's disk before anything is attempted, so a
		// message written in a car with no signal is never lost - the send
		// below simply fails and it waits for a network. Sending first and
		// queueing on failure would lose one to a crash in between.
		until_poll = 1;
	}

	bool PostBox::deliver_to(const std::string& where, const Note& note)
	{
		std::string url = where + "/msg/send?to=" + Http::escape(note.to)
			+ "&from=" + Http::escape(note.from)
			+ "&mesos=" + std::to_string(note.mesos);

		Http::Reply reply = deliver(url, note.body);

		if (!reply.ok)
			last_trouble = reply.trouble;

		return reply.ok;
	}

	bool PostBox::fetch_from(const std::string& where)
	{
		Http::Reply reply = fetch(
			where + "/msg/waiting?to=" + Http::escape(who));

		if (!reply.ok)
		{
			last_trouble = reply.trouble;

			return false;
		}

		std::vector<Note> found = parse(reply.body);

		if (found.empty())
			return true;

		std::string collected;

		{
			std::lock_guard<std::mutex> hold(files);

			for (Note& note : found)
			{
				// The id is only unique within one post box, and this device
				// may be collecting from two. Matching on the words and the
				// clock instead is what stops the same message arriving twice
				// when both the world and the relay are carrying it.
				bool already = false;

				for (const Note& have : received)
				{
					if (have.from == note.from && have.body == note.body
						&& have.sent == note.sent)
					{
						already = true;
						break;
					}
				}

				collected += std::to_string(note.id) + "\n";

				if (already)
					continue;

				received.push_back(note);
				unread_count++;

				// Said out loud, on whatever screen this device has.
				to_announce.push_back("Mail from " + note.from + ": " + note.body);

				// Writing to somebody is the strongest possible evidence you
				// know them, so a reply never needs a name typed either.
				// THROUGH THE SAME TIDY as everywhere else. This pushed the
				// sender straight in, carriage return and all.
				std::string from = tidy(note.from);

				bool had = from.empty() || from == who;

				for (const std::string& name : seen)
					had = had || (name == from);

				if (!had)
				{
					seen.push_back(from);
					save_known();
				}
			}

			save_inbox();
		}

		// SAID ONLY AFTER IT IS ON DISK. Telling the post box first and then
		// failing to write would lose the message with nobody able to notice.
		deliver(where + "/msg/collect?to=" + Http::escape(who), collected);

		return true;
	}

	void PostBox::update()
	{
		// ADDRESSED BY CHARACTER NAME, NOT ACCOUNT NAME.
		//
		// The pick-list is built from the people standing on your map, and
		// what the client knows about them is their CHARACTER name - "jubs",
		// "ianice". The account behind it ("joey", "ian") is never sent to
		// anybody else and cannot be looked up.
		//
		// So a message addressed to the account would be posted to a name
		// nobody is polling for: it would sit in the box for ever and the
		// sender would be told it went. Everybody must use the same name, and
		// the character name is the only one both ends can see.
		if (Stage::get().is_active())
		{
			const std::string& me = Stage::get().get_player().get_name();

			if (!me.empty() && me != who)
				set_account(me);
		}

		if (who.empty() || --until_poll > 0)
			return;

		until_poll = POLL_FRAMES;

		if (working.exchange(true))
			return;

		std::thread([this]()
		{
			// EVERYWHERE THAT MIGHT BE LISTENING, in order of nearness.
			//
			// The world this device is on works with no internet at all -
			// that is the thunderstorm case, and it is the one that must
			// never depend on anything. The relay is tried afterwards and
			// only if one is configured; not having one is normal.
			std::vector<std::string> boxes;

			boxes.push_back("http://127.0.0.1:8585");

			std::string ip = Setting<ServerIP>::get().load();

			if (!ip.empty() && ip != "127.0.0.1")
				boxes.push_back("http://" + ip + ":8585");

			std::string far = relay();

			if (!far.empty())
				boxes.push_back(far);

			// Out first: a message written offline should leave the moment
			// there is anywhere to leave it.
			std::vector<Note> still;

			{
				std::lock_guard<std::mutex> hold(files);
				still = pending;
			}

			std::vector<Note> unsent;

			for (const Note& note : still)
			{
				bool gone = false;

				for (const std::string& box : boxes)
				{
					if (deliver_to(box, note))
					{
						gone = true;
						break;
					}
				}

				if (!gone)
					unsent.push_back(note);
			}

			{
				std::lock_guard<std::mutex> hold(files);

				pending = unsent;
				save_outbox();
			}

			// Then in.
			for (const std::string& box : boxes)
				fetch_from(box);

			working.store(false);
		}).detach();
	}
}
