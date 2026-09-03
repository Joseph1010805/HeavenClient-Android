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
#include <mutex>
#include <unordered_set>

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

			{
				// De-duplicated by text. A player who taps a dead control
				// fifty times is reporting one bug, not fifty, and a line
				// per tap would bury the rest of the session.
				std::lock_guard<std::mutex> guard(lock);

				if (!seen.insert(line).second)
					return;
			}

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
	}
}
