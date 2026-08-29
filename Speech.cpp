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
#include "Speech.h"

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
		// One call into SpeechInput.java, where Vosk lives. SDL owns the JVM
		// and the Activity, so both are asked for rather than cached - a
		// cached Activity is how those become stale references.
		bool call_java(const char* method, const char* signature, bool pass_context)
		{
			JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());

			if (!env)
				return false;

			jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());

			if (!activity)
				return false;

			bool result = false;
			jclass cls = env->FindClass("org/heavenclient/android/SpeechInput");

			if (cls)
			{
				jmethodID id = env->GetStaticMethodID(cls, method, signature);

				if (id)
				{
					if (pass_context)
						result = env->CallStaticBooleanMethod(cls, id, activity);
					else
						env->CallStaticVoidMethod(cls, id);
				}

				env->DeleteLocalRef(cls);
			}

			// Anything thrown on the Java side has to be cleared, or the next
			// JNI call on this thread fails for no visible reason.
			if (env->ExceptionCheck())
			{
				env->ExceptionClear();
				result = false;
			}

			env->DeleteLocalRef(activity);

			return result;
		}
#endif
	}

	Speech::Speech()
	{
		listening = false;
	}

	Speech& Speech::get()
	{
		static Speech instance;
		return instance;
	}

	bool Speech::available() const
	{
#if defined(PLATFORM_ANDROID)
		return call_java("isAvailable", "(Landroid/content/Context;)Z", true);
#else
		// Desktop builds have no recogniser. Reported honestly rather than
		// pretending, so the microphone button can be hidden instead of
		// looking broken.
		return false;
#endif
	}

	bool Speech::start()
	{
#if defined(PLATFORM_ANDROID)
		{
			std::lock_guard<std::mutex> guard(lock);

			if (listening)
				return true;

			// Anything left over from a previous sentence is not this one.
			phrase.clear();
			partial.clear();
		}

		bool started = call_java("start", "(Landroid/content/Context;)Z", true);

		{
			std::lock_guard<std::mutex> guard(lock);
			listening = started;
		}

		if (!started)
			printf("[!] speech: could not start - no model, or no microphone permission\n");

		return started;
#else
		return false;
#endif
	}

	void Speech::stop()
	{
#if defined(PLATFORM_ANDROID)
		call_java("stop", "()V", false);
#endif

		std::lock_guard<std::mutex> guard(lock);
		listening = false;
	}

	bool Speech::is_listening() const
	{
		std::lock_guard<std::mutex> guard(lock);
		return listening;
	}

	std::string Speech::take_phrase()
	{
		std::lock_guard<std::mutex> guard(lock);

		std::string taken;
		taken.swap(phrase);

		return taken;
	}

	void Speech::deliver(const std::string& text)
	{
		std::lock_guard<std::mutex> guard(lock);

		phrase = text;
		listening = false;
	}

	std::string Speech::peek_partial() const
	{
		std::lock_guard<std::mutex> guard(lock);
		return partial;
	}

	void Speech::clear_partial()
	{
		std::lock_guard<std::mutex> guard(lock);
		partial.clear();
	}

	void Speech::deliver_partial(const std::string& text)
	{
		std::lock_guard<std::mutex> guard(lock);
		partial = text;
	}

	void Speech::set_listening(bool value)
	{
		std::lock_guard<std::mutex> guard(lock);
		listening = value;
	}
}

#if defined(PLATFORM_ANDROID)
extern "C"
{
	// Vosk hands back a finished sentence on its own thread. Stored, not
	// applied: the UI picks it up in its own update(), on the thread that owns
	// the text field.
	JNIEXPORT void JNICALL
	Java_org_heavenclient_android_SpeechInput_nativePhrase(
		JNIEnv* env, jclass, jstring text)
	{
		if (!text)
			return;

		const char* chars = env->GetStringUTFChars(text, nullptr);

		if (chars)
		{
			ms::Speech::get().deliver(chars);
			env->ReleaseStringUTFChars(text, chars);
		}
	}

	// The sentence as it stands, delivered many times while somebody talks.
	JNIEXPORT void JNICALL
	Java_org_heavenclient_android_SpeechInput_nativePartial(
		JNIEnv* env, jclass, jstring text)
	{
		if (!text)
			return;

		const char* chars = env->GetStringUTFChars(text, nullptr);

		if (chars)
		{
			ms::Speech::get().deliver_partial(chars);
			env->ReleaseStringUTFChars(text, chars);
		}
	}

	JNIEXPORT void JNICALL
	Java_org_heavenclient_android_SpeechInput_nativeStopped(JNIEnv*, jclass)
	{
		ms::Speech::get().set_listening(false);
	}
}
#endif
