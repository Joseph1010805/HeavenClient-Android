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
