//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "Shot.h"

#include "Silent.h"

#include <vector>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

// ⚠ NO STB_IMAGE_WRITE_IMPLEMENTATION HERE. Window_Android.cpp already
// defines it, and a second definition is a duplicate-symbol link error - the
// header is include-only everywhere except that one file.
#include <stb_image_write.h>

namespace ms
{
	namespace Shot
	{
		namespace
		{
			Which wanted = Which::NONE;
			std::string target;
			std::string taken;

			// Raised when a capture has actually reached disk, so whoever is
			// waiting to attach it does not send a file that is not there yet.
			bool ready = false;
		}

		bool take_ready()
		{
			if (!ready)
				return false;

			ready = false;

			return true;
		}

		void request(Which which, const std::string& path)
		{
			wanted = which;
			target = path;
			ready = false;
		}

		bool pending()
		{
			return wanted != Which::NONE;
		}

		const std::string& last_file()
		{
			return taken;
		}

		void collect(Which which, int32_t width, int32_t height)
		{
			if (wanted == Which::NONE || wanted != which)
				return;

			// Claimed before anything can fail, so a capture that goes wrong
			// is not retried on every frame for the rest of the session.
			wanted = Which::NONE;

			if (width <= 0 || height <= 0)
			{
				Silent::report("Shot", "asked for a "
					+ std::to_string(width) + "x" + std::to_string(height)
					+ " screen - nothing to read");

				return;
			}

			std::vector<uint8_t> pixels(
				static_cast<size_t>(width) * height * 4);

			glReadPixels(0, 0, width, height,
				GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

			// ⚠ OPENGL READS FROM THE BOTTOM LEFT, PNG WRITES FROM THE TOP.
			//
			// Without this the report arrives with an upside-down picture,
			// which is the sort of thing that makes a reader doubt the rest
			// of the file.
			stbi_flip_vertically_on_write(1);

			// Alpha is meaningless in a screenshot - the framebuffer's is
			// whatever the last blend left behind - and a PNG that carries it
			// can come out transparent in a viewer. Dropped to three
			// channels, which also makes the file a quarter smaller.
			std::vector<uint8_t> rgb(
				static_cast<size_t>(width) * height * 3);

			for (size_t i = 0, n = static_cast<size_t>(width) * height;
				i < n; i++)
			{
				rgb[i * 3 + 0] = pixels[i * 4 + 0];
				rgb[i * 3 + 1] = pixels[i * 4 + 1];
				rgb[i * 3 + 2] = pixels[i * 4 + 2];
			}

			if (stbi_write_png(target.c_str(), width, height, 3,
				rgb.data(), width * 3))
			{
				taken = target;

				ready = true;

				Silent::report("Shot", "saved " + target + " ("
					+ std::to_string(width) + "x"
					+ std::to_string(height) + ")");
			}
			else
			{
				Silent::report("Shot", "could not write " + target);
			}
		}
	}
}

// THE SHARE SHEET, WHICH IS THE WHOLE POINT.
//
// A report that stays on the device is a report nobody reads. This hands the
// files to Report.java, which offers them to whatever the player already has
// installed - see the note there on why a FileProvider is required.
#if defined(PLATFORM_ANDROID)

#include <SDL.h>
#include <jni.h>

namespace ms
{
	namespace Shot
	{
		void share(const std::string& picture)
		{
			JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());

			if (!env)
				return;

			jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());

			if (!activity)
				return;

			jclass cls = env->FindClass("org/heavenclient/android/Report");

			if (cls)
			{
				jmethodID id = env->GetStaticMethodID(cls, "share",
					"(Landroid/app/Activity;Ljava/lang/String;)V");

				if (id)
				{
					jstring name = env->NewStringUTF(picture.c_str());

					env->CallStaticVoidMethod(cls, id, activity, name);

					env->DeleteLocalRef(name);
				}

				env->DeleteLocalRef(cls);
			}

			// Anything thrown on the Java side has to be cleared, or the next
			// JNI call on this thread fails for no visible reason.
			if (env->ExceptionCheck())
				env->ExceptionClear();

			env->DeleteLocalRef(activity);
		}
	}
}

#else

namespace ms
{
	namespace Shot
	{
		void share(const std::string&) {}
	}
}

#endif
