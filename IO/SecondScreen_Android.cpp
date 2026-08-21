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
#include "SecondScreen.h"

#include "SecondScreenPanel.h"
#include "UI.h"

#include "../Graphics/GraphicsGL.h"
#include "../Graphics/Texture.h"

#if defined(PLATFORM_ANDROID)

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <jni.h>

#include "../Constants.h"

#include <nlnx/nx.hpp>

#include <atomic>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "HeavenClient", __VA_ARGS__)

namespace ms
{
	namespace SecondScreen
	{
		namespace
		{
			// Java hands the Surface over on its own thread, and EGL objects
			// belong to the thread that renders. So the window is only parked
			// here, and the render thread picks it up on its next frame.
			std::atomic<ANativeWindow*> pending{ nullptr };
			std::atomic<bool> pending_valid{ false };
			std::atomic<int> pending_width{ 0 };
			std::atomic<int> pending_height{ 0 };

			ANativeWindow* window = nullptr;
			EGLSurface surface = EGL_NO_SURFACE;
			EGLDisplay display = EGL_NO_DISPLAY;
			EGLContext context = EGL_NO_CONTEXT;

			// Where the main screen was, so it can be put back after drawing.
			EGLSurface main_draw = EGL_NO_SURFACE;
			EGLSurface main_read = EGL_NO_SURFACE;

			int16_t width = 0;
			int16_t height = 0;

			// The backdrop, loaded the first time it is wanted. It lives in
			// Map001.nx beside the login artwork - see tools/make_assets.py.
			Texture backdrop;
			bool backdrop_tried = false;

			// The last place the panel was touched, in its own pixels.
			float touch_x = 0.0f;
			float touch_y = 0.0f;

			SecondScreenPanel panel;

			void destroy_surface()
			{
				if (surface != EGL_NO_SURFACE)
				{
					eglDestroySurface(display, surface);
					surface = EGL_NO_SURFACE;
				}

				if (window)
				{
					ANativeWindow_release(window);
					window = nullptr;
				}

				width = 0;
				height = 0;
			}

			// The config the main context was made with. A surface has to
			// match its context's config, and rather than guess at one, this
			// asks the context which it was created from.
			bool find_config(EGLConfig& config)
			{
				EGLint id = 0;

				if (!eglQueryContext(display, context, EGL_CONFIG_ID, &id))
					return false;

				EGLint attribs[] = { EGL_CONFIG_ID, id, EGL_NONE };
				EGLint count = 0;

				return eglChooseConfig(display, attribs, &config, 1, &count) && count > 0;
			}

			// Called on the render thread, where the GL context is current.
			void adopt_pending()
			{
				if (!pending_valid.exchange(false))
					return;

				destroy_surface();

				ANativeWindow* next = pending.exchange(nullptr);

				if (!next)
					return;

				display = eglGetCurrentDisplay();
				context = eglGetCurrentContext();

				if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT)
				{
					LOGI("[!] second screen: no current EGL context yet");
					ANativeWindow_release(next);
					return;
				}

				EGLConfig config;

				if (!find_config(config))
				{
					LOGI("[!] second screen: could not match the context's EGL config");
					ANativeWindow_release(next);
					return;
				}

				EGLSurface next_surface = eglCreateWindowSurface(display, config, next, nullptr);

				if (next_surface == EGL_NO_SURFACE)
				{
					LOGI("[!] second screen: eglCreateWindowSurface failed, 0x%x", eglGetError());
					ANativeWindow_release(next);
					return;
				}

				window = next;
				surface = next_surface;
				width = static_cast<int16_t>(pending_width.load());
				height = static_cast<int16_t>(pending_height.load());

				LOGI("[*] second screen ready: %dx%d", width, height);
			}
		}

		bool available()
		{
			return surface != EGL_NO_SURFACE;
		}

		Point<int16_t> size()
		{
			return Point<int16_t>(width, height);
		}

		bool begin()
		{
			adopt_pending();

			if (!available())
				return false;

			main_draw = eglGetCurrentSurface(EGL_DRAW);
			main_read = eglGetCurrentSurface(EGL_READ);

			if (!eglMakeCurrent(display, surface, surface, context))
			{
				LOGI("[!] second screen: eglMakeCurrent failed, 0x%x", eglGetError());
				return false;
			}

			glViewport(0, 0, width, height);

			return true;
		}

		void draw()
		{
			if (!backdrop_tried)
			{
				backdrop_tried = true;
				backdrop = nl::nx::map001["Custom"]["BottomBg"];
			}

			GraphicsGL::get().begin_screen(WIDTH, HEIGHT);

			if (backdrop.is_valid())
				backdrop.draw(DrawArgument(Point<int16_t>(0, 0), Point<int16_t>(WIDTH, HEIGHT)));

			panel.update();
			panel.draw();

			GraphicsGL::get().flush(1.0f);
		}

		void touch(float x, float y, bool down, bool up)
		{
			touch_x = x;
			touch_y = y;

			// While the keyboard is up it covers the panel, so a touch that
			// lands on it is meant for the keyboard and nothing here.
			if (UI::get().has_focused_textfield())
				return;

			panel.send_touch(cursor(), down, up);
		}

		Point<int16_t> cursor()
		{
			if (width <= 0 || height <= 0)
				return Point<int16_t>(0, 0);

			// The panel's own pixels into the space everything is laid out in.
			return Point<int16_t>(
				static_cast<int16_t>(touch_x * WIDTH / width),
				static_cast<int16_t>(touch_y * HEIGHT / height));
		}

		void end()
		{
			eglSwapBuffers(display, surface);

			// Back to the main screen, or everything after this frame draws
			// into the wrong panel - and the shader still has to be pointed at
			// the main screen's coordinate space again.
			eglMakeCurrent(display, main_draw, main_read, context);

			GraphicsGL::get().begin_screen(
				Constants::Constants::get().get_viewwidth(),
				Constants::Constants::get().get_viewheight());
		}
	}
}

extern "C"
{
	JNIEXPORT void JNICALL
	Java_org_heavenclient_android_SecondScreen_nativeSurfaceChanged(
		JNIEnv* env, jclass, jobject jsurface, jint width, jint height)
	{
		ANativeWindow* next = ANativeWindow_fromSurface(env, jsurface);

		ms::SecondScreen::pending.exchange(next);
		ms::SecondScreen::pending_width.store(width);
		ms::SecondScreen::pending_height.store(height);
		ms::SecondScreen::pending_valid.store(true);
	}

	JNIEXPORT void JNICALL
	Java_org_heavenclient_android_SecondScreen_nativeSurfaceDestroyed(JNIEnv*, jclass)
	{
		ms::SecondScreen::pending.exchange(nullptr);
		ms::SecondScreen::pending_valid.store(true);
	}

	JNIEXPORT void JNICALL
	Java_org_heavenclient_android_SecondScreen_nativeTouch(
		JNIEnv*, jclass, jfloat x, jfloat y, jboolean down, jboolean up)
	{
		ms::SecondScreen::touch(x, y, down == JNI_TRUE, up == JNI_TRUE);
	}
}

#endif
