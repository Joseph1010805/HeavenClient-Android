//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "Carry.h"

#include "Silent.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>

#define BOOST_DATE_TIME_NO_LIB
#define BOOST_REGEX_NO_LIB
#include "asio.hpp"

namespace ms
{
	namespace
	{
		// The carry port. Deliberately not the game's - a refused connection
		// here means visiting is off, not that the world is down.
		constexpr const char* CARRY_PORT = "8585";

		// This device's own server. Reached over loopback, which works with
		// no network at all - in a car, on a plane, anywhere.
		constexpr const char* HOME = "127.0.0.1";

		// A minute, near enough, at sixty frames. The server autosaves on the
		// same beat, so a finer timer only copies the same rows again.
		constexpr int32_t REFRESH_FRAMES = 60 * 60;

		// Long enough for a handheld to answer over wifi, short enough that a
		// host which has gone away does not hold up the game.
		constexpr int TIMEOUT_MS = 4000;
	}

	std::string Carry::card_path()
	{
		// The app's own folder: it survives the app closing and the device
		// rebooting, and needs no permission that Android might take away.
		return "/sdcard/Android/data/org.heavenclient.android/files/"
			"HeavenClient/carry.card";
	}

	bool Carry::has_card() const
	{
		std::ifstream in(card_path(), std::ios::binary);

		return in.good() && in.peek() != std::ifstream::traits_type::eof();
	}

	void Carry::set_account(const std::string& name)
	{
		who = name;
	}

	// ONE REQUEST, SYNCHRONOUSLY, WITH A DEADLINE.
	//
	// Written out rather than pulled in: the client already carries asio and
	// nothing else here needs HTTP, so a whole library would be a dependency
	// for three verbs. It speaks only as much as the carry port answers -
	// no chunked encoding, no keep-alive, no redirects.
	//
	// A DEADLINE IS THE POINT. A handheld that has walked out of range does
	// not refuse a connection, it simply never answers; without a timer this
	// would hold the frame for as long as the operating system felt like it.
	std::string Carry::request(const std::string& host, const std::string& method,
		const std::string& path, const std::string& body, bool& ok)
	{
		ok = false;

		try
		{
			asio::io_context io;
			asio::ip::tcp::socket socket(io);
			asio::ip::tcp::resolver resolver(io);

			auto endpoints = resolver.resolve(host, CARRY_PORT);

			// asio's synchronous connect has no timeout of its own, so the
			// whole exchange runs on the io_context with a deadline over it.
			std::error_code result = asio::error::would_block;

			asio::async_connect(socket, endpoints,
				[&](const std::error_code& e, const asio::ip::tcp::endpoint&)
				{
					result = e;
				});

			io.run_for(std::chrono::milliseconds(TIMEOUT_MS));

			if (result || !socket.is_open())
			{
				last_trouble = "could not reach " + host;

				return {};
			}

			std::ostringstream out;

			out << method << ' ' << path << " HTTP/1.1\r\n"
				<< "Host: " << host << "\r\n"
				<< "Connection: close\r\n"
				<< "Content-Type: text/plain; charset=utf-8\r\n"
				<< "Content-Length: " << body.size() << "\r\n\r\n"
				<< body;

			std::string wire = out.str();

			asio::write(socket, asio::buffer(wire), result);

			if (result)
			{
				last_trouble = "could not send to " + host;

				return {};
			}

			// Read to the end of the connection - the server said
			// Connection: close, so that is the whole answer.
			asio::streambuf incoming;
			std::error_code read_result;

			asio::read(socket, incoming, read_result);

			if (read_result && read_result != asio::error::eof)
			{
				last_trouble = "lost the connection to " + host;

				return {};
			}

			std::string reply(
				asio::buffers_begin(incoming.data()),
				asio::buffers_end(incoming.data()));

			size_t split = reply.find("\r\n\r\n");

			if (split == std::string::npos)
			{
				last_trouble = host + " answered with something unreadable";

				return {};
			}

			std::string head = reply.substr(0, split);
			std::string content = reply.substr(split + 4);

			// THE STATUS LINE DECIDES, NOT THE PRESENCE OF A BODY.
			//
			// A 404 has a body too - "no account called joey" - and writing
			// that over the card would replace a character with a sentence.
			ok = head.compare(0, 12, "HTTP/1.1 200") == 0;

			if (!ok)
			{
				size_t line = head.find("\r\n");

				last_trouble = head.substr(0, line == std::string::npos ? head.size() : line)
					+ " - " + content.substr(0, 120);
			}

			return content;
		}
		catch (const std::exception& e)
		{
			last_trouble = std::string("carry failed: ") + e.what();

			return {};
		}
	}

	bool Carry::reachable(const std::string& host)
	{
		bool ok = false;

		std::string said = request(host.empty() ? HOME : host, "GET",
			"/carry/health", "", ok);

		return ok && said.rfind("ok ", 0) == 0;
	}

	bool Carry::fetch(const std::string& host)
	{
		if (who.empty())
		{
			last_trouble = "no account to fetch";

			return false;
		}

		bool ok = false;

		std::string blob = request(host.empty() ? HOME : host, "GET",
			"/carry/export?account=" + who, "", ok);

		if (!ok || blob.empty())
		{
			Silent::report("Carry", "could not fetch the card: " + last_trouble);

			return false;
		}

		// WRITTEN BESIDE, THEN MOVED OVER.
		//
		// A card half-written is a character half-lost, and the moment this
		// is most likely to be interrupted - closing the game, a flat battery
		// - is exactly when it is being written. Renaming over the old one is
		// the only step that cannot leave a ruin behind.
		std::lock_guard<std::mutex> hold(card_lock);

		std::string temp = card_path() + ".new";

		{
			std::ofstream out(temp, std::ios::binary | std::ios::trunc);

			if (!out)
			{
				last_trouble = "could not write the card";

				Silent::report("Carry", last_trouble);

				return false;
			}

			out << blob;

			if (!out)
			{
				last_trouble = "the card did not write fully";

				Silent::report("Carry", last_trouble);

				return false;
			}
		}

		std::remove(card_path().c_str());

		if (std::rename(temp.c_str(), card_path().c_str()) != 0)
		{
			last_trouble = "could not replace the card";

			Silent::report("Carry", last_trouble);

			return false;
		}

		return true;
	}

	bool Carry::deliver(const std::string& host)
	{
		std::ifstream in(card_path(), std::ios::binary);

		if (!in)
		{
			last_trouble = "there is no card on this device yet";

			return false;
		}

		std::stringstream buffer;
		buffer << in.rdbuf();

		std::string blob = buffer.str();

		if (blob.empty())
		{
			last_trouble = "the card is empty";

			return false;
		}

		bool ok = false;

		request(host, "POST", "/carry/import", blob, ok);

		if (!ok)
		{
			Silent::report("Carry", "could not hand the card over: " + last_trouble);
		}

		return ok;
	}

	void Carry::update()
	{
		// NOT WHILE VISITING. Away from home the host owns these characters,
		// and refreshing from here would fetch from the wrong database - or
		// worse, from our own stale one.
		if (away_from_home || who.empty())
		{
			return;
		}

		if (--until_refresh > 0)
		{
			return;
		}

		until_refresh = REFRESH_FRAMES;

		// OFF THE FRAME. A request waits up to four seconds for an answer and
		// an unreachable device uses every one of them; done here that is a
		// four-second freeze in the middle of play.
		//
		// Quietly, too: this runs while somebody is playing and must never be
		// the reason a message appears. If the card cannot be refreshed now it
		// is tried again in a minute, and Silent has written down why.
		if (working.exchange(true))
		{
			return;
		}

		std::thread([this]()
		{
			fetch("");

			working.store(false);
		}).detach();
	}

	void Carry::come_home()
	{
		if (!away_from_home || away_host.empty() || who.empty())
		{
			// Not a visit, so there is nothing out there to collect.
			away_from_home = false;

			return;
		}

		std::string from = away_host;

		// Marked home immediately, so the refresh timer takes over even if
		// the fetch below fails - otherwise a device that lost its host would
		// stop keeping its own card for ever.
		away_from_home = false;
		away_host.clear();

		if (working.exchange(true))
		{
			return;
		}

		std::thread([this, from]()
		{
			// A FEW TRIES, BECAUSE THE SERVER IS STILL WRITING.
			//
			// Cosmic saves a character when it disconnects, and this runs the
			// instant the connection drops - so the first ask can easily
			// arrive before the row has been written. Asking once would fetch
			// the previous autosave and silently lose the last minute of play.
			//
			// If the host has genuinely gone away all three fail, the card is
            // left exactly as it was, and Silent says so.
			for (int attempt = 0; attempt < 3; attempt++)
			{
				if (attempt > 0)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1500));
				}

				if (fetch(from))
				{
					working.store(false);

					return;
				}
			}

			Silent::report("Carry",
				"could not bring the characters home from " + from
				+ " - they are still on that world: " + last_trouble);

			working.store(false);
		}).detach();
	}

	void Carry::set_visiting(bool away, const std::string& host)
	{
		away_from_home = away;
		away_host = host;

		// Home again - take a copy as soon as the timer next comes round
		// rather than waiting a full minute.
		if (!away)
		{
			until_refresh = 1;
		}
	}
}
