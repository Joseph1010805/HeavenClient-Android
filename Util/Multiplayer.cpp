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
//////////////////////////////////////////////////////////////////////////////////
#include "Multiplayer.h"

#ifdef PLATFORM_ANDROID
#include <jni.h>

#include <SDL.h>
#endif

namespace ms
{
	namespace Multiplayer
	{
#ifdef PLATFORM_ANDROID
		namespace
		{
			// SDL owns the JVM and the Activity. Both are fetched per call
			// rather than cached: a cached Activity across a configuration
			// change is a stale reference waiting to happen.
			struct Env
			{
				JNIEnv* env = nullptr;
				jobject activity = nullptr;

				Env()
				{
					env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());

					if (env)
						activity = static_cast<jobject>(SDL_AndroidGetActivity());
				}

				~Env()
				{
					if (env && activity)
						env->DeleteLocalRef(activity);
				}

				explicit operator bool() const { return env && activity; }

				// Anything thrown on the Java side must be cleared, or the
				// next JNI call on this thread fails for no visible reason.
				void clear_exception()
				{
					if (env && env->ExceptionCheck())
						env->ExceptionClear();
				}
			};

			bool call_bool(const char* cls_name, const char* method,
				const char* sig, bool with_context = true)
			{
				Env e;

				if (!e)
					return false;

				bool result = false;
				jclass cls = e.env->FindClass(cls_name);

				if (cls)
				{
					jmethodID id = e.env->GetStaticMethodID(cls, method, sig);

					if (id)
					{
						result = with_context
							? e.env->CallStaticBooleanMethod(cls, id, e.activity)
							: e.env->CallStaticBooleanMethod(cls, id);
					}

					e.env->DeleteLocalRef(cls);
				}

				e.clear_exception();

				return result;
			}

			void call_void(const char* cls_name, const char* method)
			{
				Env e;

				if (!e)
					return;

				jclass cls = e.env->FindClass(cls_name);

				if (cls)
				{
					jmethodID id = e.env->GetStaticMethodID(
						cls, method, "(Landroid/content/Context;)V");

					if (id)
						e.env->CallStaticVoidMethod(cls, id, e.activity);

					e.env->DeleteLocalRef(cls);
				}

				e.clear_exception();
			}

			std::string to_string(JNIEnv* env, jstring str)
			{
				if (!str)
					return "";

				const char* chars = env->GetStringUTFChars(str, nullptr);
				std::string out = chars ? chars : "";

				if (chars)
					env->ReleaseStringUTFChars(str, chars);

				return out;
			}
		}
#endif

		bool start_hosting(const std::string& name)
		{
#ifdef PLATFORM_ANDROID
			Env e;

			if (!e)
				return false;

			bool result = false;
			jclass cls = e.env->FindClass("org/heavenclient/android/Discovery");

			if (cls)
			{
				jmethodID id = e.env->GetStaticMethodID(cls, "host",
					"(Landroid/content/Context;Ljava/lang/String;I)Z");

				if (id)
				{
					jstring jname = e.env->NewStringUTF(name.c_str());

					// 8484 is the login port, which is the one a joining
					// client actually dials.
					result = e.env->CallStaticBooleanMethod(
						cls, id, e.activity, jname, 8484);

					e.env->DeleteLocalRef(jname);
				}

				e.env->DeleteLocalRef(cls);
			}

			e.clear_exception();

			return result;
#else
			(void)name;
			return false;
#endif
		}

		void stop_hosting()
		{
#ifdef PLATFORM_ANDROID
			call_void("org/heavenclient/android/Discovery", "stopHosting");
#endif
		}

		bool start_browsing()
		{
#ifdef PLATFORM_ANDROID
			return call_bool("org/heavenclient/android/Discovery", "browse",
				"(Landroid/content/Context;)Z");
#else
			return false;
#endif
		}

		void stop_browsing()
		{
#ifdef PLATFORM_ANDROID
			call_void("org/heavenclient/android/Discovery", "stopBrowsing");
#endif
		}

		std::vector<Game> games()
		{
			std::vector<Game> out;

#ifdef PLATFORM_ANDROID
			Env e;

			if (!e)
				return out;

			jclass cls = e.env->FindClass("org/heavenclient/android/Discovery");

			if (cls)
			{
				jmethodID id = e.env->GetStaticMethodID(
					cls, "list", "()[Ljava/lang/String;");

				if (id)
				{
					jobjectArray arr = static_cast<jobjectArray>(
						e.env->CallStaticObjectMethod(cls, id));

					if (arr)
					{
						jsize count = e.env->GetArrayLength(arr);

						for (jsize i = 0; i < count; i++)
						{
							jstring item = static_cast<jstring>(
								e.env->GetObjectArrayElement(arr, i));

							// "name<tab>address" - the cheapest thing to hand
							// across JNI, and unambiguous because a service
							// name cannot contain a tab.
							std::string line = to_string(e.env, item);
							size_t split = line.find('\t');

							if (split != std::string::npos)
								out.push_back({ line.substr(0, split),
									line.substr(split + 1) });

							e.env->DeleteLocalRef(item);
						}

						e.env->DeleteLocalRef(arr);
					}
				}

				e.env->DeleteLocalRef(cls);
			}

			e.clear_exception();
#endif

			return out;
		}

		std::string suggested_name()
		{
#ifdef PLATFORM_ANDROID
			Env e;

			if (!e)
				return "Maple Server";

			std::string out = "Maple Server";
			jclass cls = e.env->FindClass("org/heavenclient/android/Discovery");

			if (cls)
			{
				jmethodID id = e.env->GetStaticMethodID(
					cls, "suggestedName", "()Ljava/lang/String;");

				if (id)
				{
					jstring name = static_cast<jstring>(
						e.env->CallStaticObjectMethod(cls, id));

					if (name)
					{
						out = to_string(e.env, name);
						e.env->DeleteLocalRef(name);
					}
				}

				e.env->DeleteLocalRef(cls);
			}

			e.clear_exception();

			return out;
#else
			return "Maple Server";
#endif
		}

		bool wifi_direct_supported()
		{
#ifdef PLATFORM_ANDROID
			return call_bool("org/heavenclient/android/WifiDirect", "isSupported",
				"(Landroid/content/Context;)Z");
#else
			return false;
#endif
		}

		bool create_group()
		{
#ifdef PLATFORM_ANDROID
			return call_bool("org/heavenclient/android/WifiDirect", "createGroup",
				"(Landroid/content/Context;)Z");
#else
			return false;
#endif
		}

		void remove_group()
		{
#ifdef PLATFORM_ANDROID
			call_void("org/heavenclient/android/WifiDirect", "removeGroup");
#endif
		}

		bool find_groups()
		{
#ifdef PLATFORM_ANDROID
			return call_bool("org/heavenclient/android/WifiDirect", "discoverPeers",
				"(Landroid/content/Context;)Z");
#else
			return false;
#endif
		}
	}
}
