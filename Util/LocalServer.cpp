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
#include "LocalServer.h"

#include "../Configuration.h"

#ifdef PLATFORM_ANDROID
#include <jni.h>

#include <SDL.h>
#endif

namespace ms
{
	namespace LocalServer
	{
#ifdef PLATFORM_ANDROID
		namespace
		{
			// One call into LocalServer.java, which is where the Termux
			// business lives. SDL owns the JVM and the Activity, so both come
			// from it rather than being cached - caching an Activity across a
			// configuration change is how those turn into stale references.
			bool call_java(const char* method)
			{
				JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());

				if (!env)
					return false;

				jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());

				if (!activity)
					return false;

				bool result = false;
				jclass cls = env->FindClass("org/heavenclient/android/LocalServer");

				if (cls)
				{
					jmethodID id = env->GetStaticMethodID(
						cls, method, "(Landroid/content/Context;)Z");

					if (id)
						result = env->CallStaticBooleanMethod(cls, id, activity);

					env->DeleteLocalRef(cls);
				}

				// Anything thrown on the Java side has to be cleared or the
				// next JNI call in this thread fails for no visible reason.
				if (env->ExceptionCheck())
				{
					env->ExceptionClear();
					result = false;
				}

				env->DeleteLocalRef(activity);

				return result;
			}
		}
#endif

		bool is_offline()
		{
			return Setting<ServerIP>::get().load() == HERE;
		}

		std::string home_address()
		{
			return Setting<HomeServerIP>::get().load();
		}

		void set_home_address(const std::string& address)
		{
			Setting<HomeServerIP>::get().save(address);
		}

		void set_offline(bool offline)
		{
			if (offline)
			{
				// Remember the way back before covering it over. Doing this
				// only when actually leaving home means a second tap of the
				// switch cannot overwrite the real address with 127.0.0.1.
				if (!is_offline())
					set_home_address(Setting<ServerIP>::get().load());

				Setting<ServerIP>::get().save(HERE);
			}
			else
			{
				std::string home = home_address();

				Setting<ServerIP>::get().save(home.empty() ? "192.168.1.71" : home);
			}

			// Written out now rather than at exit: the point of the switch is
			// to survive the app being closed and reopened.
			Configuration::get().save();
		}

		bool can_host()
		{
#ifdef PLATFORM_ANDROID
			return call_java("isAvailable");
#else
			return false;
#endif
		}

		bool start()
		{
#ifdef PLATFORM_ANDROID
			return call_java("start");
#else
			return false;
#endif
		}
	}
}
