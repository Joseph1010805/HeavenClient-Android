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

#include "../Gameplay/Stage.h"

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
#include <memory>
#include <mutex>
#include <vector>

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

			// Touches waiting to be handled on the game thread.
			//
			// Android delivers them on ITS ui thread, and the game - including
			// every NX read and every atlas upload - runs on the SDL thread,
			// which is the only thread the GL context is current on. Handling a
			// touch where it arrives meant that tapping a region of the world
			// map ran update_world() on the wrong thread: the NX read raced the
			// renderer's own reads on one shared FILE handle, and the texture
			// upload called into GL with no context, which quietly does nothing
			// while still recording where the picture was meant to have gone.
			// The region then drew as an empty rectangle - unless the top
			// screen had already loaded that same region properly, which is
			// exactly the shape of the bug that was reported.
			//
			// So a touch is only recorded here and acted on in draw().
			struct PendingTouch
			{
				float x;
				float y;
				bool down;
				bool up;
			};

			std::mutex touch_lock;
			std::vector<PendingTouch> pending_touches;

			void deliver_touches();

			// Built on first use, not at load time.
			//
			// A namespace-scope object here would be constructed during static
			// initialisation, before the NX data is open - so every texture it
			// looked up would be null and stay null, which is exactly why the
			// page arrows never appeared.
			std::unique_ptr<SecondScreenPanel> panel_ptr;

			SecondScreenPanel& get_panel()
			{
				if (!panel_ptr)
					panel_ptr = std::make_unique<SecondScreenPanel>();

				return *panel_ptr;
			}

			// The space the panel is laid out in.
			//
			// Not its raw pixels: the main screen draws an 800x600 design onto
			// a 1920x1080 panel, so everything there is enlarged about 1.8
			// times. Drawing this panel one to one made the same buttons and
			// text half the size they are on the top screen and too small to
			// read. Matching that 600-high design keeps them the size the
			// player already knows, and the width follows the panel's shape.
			Point<int16_t> layout_size()
			{
				// Half what the main screen uses, which makes everything on
				// this panel twice the size. The lower screen is looked at
				// from further away and tapped with a thumb rather than
				// pointed at with a cursor, so its controls want to be bigger
				// than the same controls on the top screen, not the same size.
				constexpr int16_t DESIGN_HEIGHT = 300;

				if (height <= 0)
					return Point<int16_t>(DESIGN_HEIGHT, DESIGN_HEIGHT);

				return Point<int16_t>(
					static_cast<int16_t>(width * DESIGN_HEIGHT / height),
					DESIGN_HEIGHT);
			}

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

			Point<int16_t> space = layout_size();

			// Black behind everything. A map loading used to flash the panel
			// white, which is the harshest thing a screen this close to the eye
			// can do.
			GraphicsGL::get().set_clearcolour(0.0f, 0.0f, 0.0f);

			GraphicsGL::get().begin_screen(space.x(), space.y());

			// The forest belongs to the login screen and nowhere else. In game
			// the panel is a working surface: a page either brings its own
			// backdrop - the inventory brings the bag - or sits on black, and
			// either way a woodland scene behind the slots is just noise.
			if (backdrop.is_valid() && !Stage::get().is_active())
				backdrop.draw(DrawArgument(Point<int16_t>(0, 0), space));

			// On the game thread, with the GL context current - which is the
			// whole point of queueing them.
			deliver_touches();

			get_panel().update();
			get_panel().draw(space);

			GraphicsGL::get().flush(1.0f);

			// Put the main screen's own clear colour back, or the game flashes
			// brown instead.
			GraphicsGL::get().set_clearcolour(1.0f, 1.0f, 1.0f);
		}

		void touch(float x, float y, bool down, bool up)
		{
			// Called on Android's ui thread. Record it and get out - see
			// pending_touches. Nothing here may read or write game state.
			std::lock_guard<std::mutex> guard(touch_lock);

			pending_touches.push_back({ x, y, down, up });
		}

		namespace
		{
			// Hand the touches that have arrived since the last frame to the
			// panel, on the game thread.
			void deliver_touches()
			{
				std::vector<PendingTouch> touches;

				{
					std::lock_guard<std::mutex> guard(touch_lock);

					touches.swap(pending_touches);
				}

				for (const PendingTouch& t : touches)
				{
					touch_x = t.x;
					touch_y = t.y;

					// While the keyboard is up it covers the panel, so a touch
					// that lands on it is meant for the keyboard and nothing
					// here.
					if (UI::get().has_focused_textfield())
						continue;

					get_panel().send_touch(cursor(), layout_size(), t.down, t.up);
				}
			}
		}

		void play_levelup()
		{
			if (!available() || !panel_ptr)
				return;

			panel_ptr->play_levelup();
		}

		void draw_top_tooltip()
		{
			if (!available() || !panel_ptr)
				return;

			panel_ptr->draw_top_tooltip();
		}

		UIElement* hosted(UIElement::Type type)
		{
			// Deliberately not get_panel(): asking whether a window exists must
			// not be what brings the panel into being.
			if (!panel_ptr)
				return nullptr;

			return panel_ptr->hosted(type);
		}

		Keyboard::Mapping selected_mapping()
		{
			// Same reasoning as hosted(): a question must not build a panel.
			if (!panel_ptr)
				return {};

			return panel_ptr->selected_mapping();
		}

		Point<int16_t> cursor()
		{
			if (width <= 0 || height <= 0)
				return Point<int16_t>(0, 0);

			Point<int16_t> space = layout_size();

			return Point<int16_t>(
				static_cast<int16_t>(touch_x * space.x() / width),
				static_cast<int16_t>(touch_y * space.y() / height));
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
