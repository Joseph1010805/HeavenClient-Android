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
#include "Voice.h"

#include <cstdio>

#if defined(PLATFORM_ANDROID)
#include <SDL.h>
#include <jni.h>
#endif

namespace ms
{
	namespace
	{
#if defined(PLATFORM_ANDROID)
		// The same shape as Speech's bridge, and for the same reason: SDL owns
		// the JVM and the Activity, so both are asked for every call rather
		// than cached. A cached Activity is how these go stale.
		bool call_java(const char* method, const char* signature,
			bool pass_context, bool pass_flag = false, bool flag = false)
		{
			JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());

			if (!env)
				return false;

			jclass cls = env->FindClass("org/heavenclient/android/VoiceChat");

			if (!cls)
			{
				env->ExceptionClear();

				return false;
			}

			bool result = false;
			jmethodID id = env->GetStaticMethodID(cls, method, signature);

			if (id)
			{
				if (pass_context)
				{
					jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());

					if (activity)
					{
						result = env->CallStaticBooleanMethod(cls, id, activity);

						env->DeleteLocalRef(activity);
					}
				}
				else if (pass_flag)
				{
					env->CallStaticVoidMethod(cls, id, flag ? JNI_TRUE : JNI_FALSE);
				}
				else
				{
					env->CallStaticVoidMethod(cls, id);
				}
			}

			if (env->ExceptionCheck())
				env->ExceptionClear();

			env->DeleteLocalRef(cls);

			return result;
		}
#endif
	}

	Voice& Voice::get()
	{
		static Voice instance;

		return instance;
	}

	bool Voice::start()
	{
		if (open)
			return true;

#if defined(PLATFORM_ANDROID)
		open = call_java("start", "(Landroid/content/Context;)Z", true);

		if (!open)
			printf("[!] voice: could not start - no permission, or speech to text has the microphone\n");
#else
		// Nothing to talk to. Reported rather than pretended, so the page can
		// say so instead of showing a button that does nothing.
		printf("[ ] voice: not available on this platform\n");
#endif

		return open;
	}

	void Voice::stop()
	{
		if (!open)
			return;

		talking = false;

#if defined(PLATFORM_ANDROID)
		call_java("setTransmitting", "(Z)V", false, true, false);
		call_java("stop", "()V", false);
#endif

		open = false;
	}

	bool Voice::is_open() const
	{
		return open;
	}

	void Voice::set_talking(bool on)
	{
		// Never transmit while the socket is shut - the Java would ignore it,
		// but the button would light up as though it were live.
		if (!open)
			on = false;

		if (on == talking)
			return;

		talking = on;

#if defined(PLATFORM_ANDROID)
		call_java("setTransmitting", "(Z)V", false, true, on);
#endif
	}

	bool Voice::is_talking() const
	{
		return talking;
	}
}
