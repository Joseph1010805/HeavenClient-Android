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
#include "Silent.h"

#include <cstdio>
#include <ctime>
#include <iostream>
#include <algorithm>
#include <deque>
#include <map>
#include <mutex>
#include <unordered_set>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace ms
{
	namespace Silent
	{
		namespace
		{
			std::mutex lock;
			std::unordered_set<std::string> seen;

			// HOW OFTEN, AND WHEN. See Silent::snapshot.
			//
			// report() only writes a line the first time, which is right for
			// the log and wrong for diagnosis: "this happened once" and "this
			// happened four hundred times" are different bugs, and the second
			// number is usually the one that names the cause.
			struct Tally
			{
				size_t count = 0;
				std::time_t first = 0;
				std::time_t last = 0;
			};

			std::map<std::string, Tally> tally;

			// THE LAST FEW HUNDRED EVENTS, IN ORDER, NOT DE-DUPLICATED.
			//
			// Order is the thing dedupe destroys, and order is how almost
			// every fault this month was actually identified - the mute NPC,
			// the chair, the storage keeper. Bounded so an evening of play
			// cannot grow without limit.
			struct Event
			{
				std::time_t when;
				std::string line;
			};

			std::deque<Event> recent;

			constexpr size_t RECENT_MAX = 600;

			std::string stamp(std::time_t when, const char* format)
			{
				std::tm local {};

#ifdef _WIN32
				localtime_s(&local, &when);
#else
				localtime_r(&when, &local);
#endif

				char text[32];
				std::strftime(text, sizeof(text), format, &local);

				return text;
			}

			// WHERE THE SESSION'S LIST SURVIVES THE SESSION.
			//
			// logcat is a ring buffer. An evening of play overruns it, the
			// device gets unplugged, and by the time anybody asks what went
			// wrong the answer has been overwritten by chatter from Google
			// Play. Every one of these lines is a bug nobody has noticed yet,
			// which makes them exactly the wrong thing to keep somewhere
			// temporary.
			//
			// This is the app's own external folder - the one holding the NX
			// data and the Settings file - so it needs no permission and is
			// readable with a plain `adb shell cat`, no run-as. See
			// tools/playlog.py, which collects it alongside the server's.
			constexpr const char* FOLDER =
				"/sdcard/Android/data/org.heavenclient.android/files/HeavenClient/";

			constexpr const char* PLAYLOG =
				"/sdcard/Android/data/org.heavenclient.android/files/HeavenClient/playlog.txt";

			void keep(const std::string& line)
			{
				// Appended, never truncated: two sessions before somebody
				// asks is the normal case, and the earlier one is usually
				// the one that matters.
				std::FILE* out = std::fopen(PLAYLOG, "a");

				if (!out)
					return;

				std::time_t now = std::time(nullptr);
				std::tm local {};

#ifdef _WIN32
				localtime_s(&local, &now);
#else
				localtime_r(&now, &local);
#endif

				char when[32];
				std::strftime(when, sizeof(when), "%m-%d %H:%M:%S", &local);

				std::fprintf(out, "%s CLIENT %s\n", when, line.c_str());
				std::fclose(out);
			}
		}

		void report(const char* where, const std::string& what)
		{
			std::string line = std::string(where) + ": " + what;

			bool first_time;

			{
				// De-duplicated by text for the LOG - a player who taps a
				// dead control fifty times is reporting one bug, not fifty.
				// But counted every time, and remembered in order, because a
				// snapshot wants both of those and the log wants neither.
				std::lock_guard<std::mutex> guard(lock);

				std::time_t now = std::time(nullptr);

				Tally& t = tally[line];

				if (t.count == 0)
					t.first = now;

				t.count++;
				t.last = now;

				recent.push_back({ now, line });

				while (recent.size() > RECENT_MAX)
					recent.pop_front();

				first_time = seen.insert(line).second;
			}

			if (!first_time)
				return;

#ifdef __ANDROID__
			// Its own tag, so the session's whole list comes out of
			// `adb logcat -s HeavenSilent:I` without anything else in it.
			__android_log_print(ANDROID_LOG_INFO, "HeavenSilent", "%s", line.c_str());

			// AND on disk, because logcat will not be there tomorrow.
			keep(line);
#else
			std::cout << "[silent] " << line << std::endl;
#endif
		}

		size_t count()
		{
			std::lock_guard<std::mutex> guard(lock);

			return seen.size();
		}

		std::string snapshot(const std::string& note)
		{
			// BESIDE THE PLAY LOG, in the app's own external folder - no
			// permission needed and readable with a plain `adb shell cat`.
			// One file per snapshot, named by the clock, so a long evening
			// can leave several and none overwrites another.
			std::string path = std::string(FOLDER) + "report-"
				+ stamp(std::time(nullptr), "%m%d-%H%M%S") + ".txt";

			std::FILE* out = std::fopen(path.c_str(), "w");

			if (!out)
				return "";

			std::vector<std::pair<std::string, Tally>> rows;
			std::deque<Event> tail;

			{
				std::lock_guard<std::mutex> guard(lock);

				rows.assign(tally.begin(), tally.end());
				tail = recent;
			}

			// LOUDEST FIRST. The line that fired four hundred times is
			// almost always the one worth reading, and a list sorted
			// alphabetically buries it among the ones that fired once.
			std::sort(rows.begin(), rows.end(),
				[](const std::pair<std::string, Tally>& a,
					const std::pair<std::string, Tally>& b)
				{
					return a.second.count > b.second.count;
				});

			std::fprintf(out, "LocalStory session report\n");
			std::fprintf(out, "taken %s\n",
				stamp(std::time(nullptr), "%Y-%m-%d %H:%M:%S").c_str());

			if (!note.empty())
				std::fprintf(out, "\nWHAT THE PLAYER SAID\n  %s\n",
					note.c_str());

			std::fprintf(out, "\nWHAT THE CLIENT NOTICED - %d distinct, "
				"loudest first\n", static_cast<int>(rows.size()));

			std::fprintf(out, "  %8s  %8s  %8s  %s\n",
				"count", "first", "last", "line");

			for (const auto& row : rows)
				std::fprintf(out, "  %8d  %8s  %8s  %s\n",
					static_cast<int>(row.second.count),
					stamp(row.second.first, "%H:%M:%S").c_str(),
					stamp(row.second.last, "%H:%M:%S").c_str(),
					row.first.c_str());

			// AND THE ORDER IT ALL HAPPENED IN.
			//
			// The table says what and how often; this says what came just
			// before what, which is how nearly every fault this month was
			// actually pinned down.
			std::fprintf(out, "\nIN ORDER - last %d events\n",
				static_cast<int>(tail.size()));

			for (const Event& e : tail)
				std::fprintf(out, "  %s  %s\n",
					stamp(e.when, "%H:%M:%S").c_str(), e.line.c_str());

			std::fclose(out);

			return path;
		}
	}
}
