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

#include "../Graphics/Geometry.h"
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

			// The panel repoints the shared renderer at its own screen. If the
			// main screen comes out crooked, this is the most likely thing to
			// have left it that way - so it says what it set.
			{
				static int last_w = -1, last_h = -1;

				if (width != last_w || height != last_h)
				{
					LOGI("panel viewport: %dx%d", width, height);

					last_w = width; last_h = height;
				}
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

					// EVERY TOUCH GOES TO THE PANEL.
					//
					// This used to drop the touch whenever a textfield had
					// focus, on the reasoning that Android's own keyboard was
					// covering the panel and the press belonged to it.
					//
					// That reasoning inverted the day the panel grew a
					// keyboard of its own. A focused textfield is now the one
					// state in which a press on this screen is CERTAIN to be
					// meant for the panel - it is somebody typing - and this
					// line threw exactly those presses away. With no field
					// focused the keys reached the panel and had nowhere to
					// deliver a character to; with one focused they never
					// reached it at all. Either way nothing was ever typed,
					// which is what "the keyboard does nothing" was.
					//
					// Android's keyboard no longer opens here at all - see
					// Window_Android.cpp, which stops text input from starting
					// on a device that has a second screen.
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

		// ---- the same panel, over the game, on a one-screen device --------

		namespace
		{
			bool overlay = false;

			// The space the overlay lays out in: the main screen's own design
			// pixels, so it lands exactly over the game and needs no scaling
			// of its own.
			//
			// NOT layout_size(). That halves the height deliberately, because
			// the real panel is a separate screen looked at from further away
			// and tapped with a thumb. This one is the screen the player is
			// already looking at, at the size the rest of the interface is
			// drawn at, and doubling it here would push the pages off the
			// edges - the world map's frame alone is 654x537.
			Point<int16_t> overlay_size()
			{
				return Point<int16_t>(
					Constants::Constants::get().get_viewwidth(),
					Constants::Constants::get().get_viewheight());
			}
		}

		bool overlay_supported()
		{
			return !available();
		}

		bool overlay_showing()
		{
			return overlay && overlay_supported();
		}

		void toggle_overlay()
		{
			if (!overlay_supported())
				return;

			overlay = !overlay;
		}

		bool overlay_alert()
		{
			// panel_ptr, not get_panel(): asking whether there is post must
			// not be the thing that builds the panel, and this is asked every
			// frame the status bar draws.
			if (!overlay_supported() || !panel_ptr)
				return false;

			return panel_ptr->any_alert();
		}

		bool open_overlay(Section section)
		{
			if (!overlay_supported())
				return false;

			SecondScreenPanel::Page page = SecondScreenPanel::HOME;

			switch (section)
			{
			case Section::CHARACTER:  page = SecondScreenPanel::CHARACTER; break;
			case Section::ADVENTURE:  page = SecondScreenPanel::ADVENTURE; break;
			case Section::SOCIAL:     page = SecondScreenPanel::CHAT;      break;
			case Section::SETTINGS:   page = SecondScreenPanel::SETTINGS;  break;
			case Section::DAILY:      page = SecondScreenPanel::DAILY;     break;
			case Section::HOME:       page = SecondScreenPanel::HOME;      break;
			}

			// FROM HOME, so the way out is the way in. Descending from
			// wherever the panel happened to be left would build a trail that
			// does not match how the player got here, and a back swipe would
			// climb through pages they never opened.
			get_panel().go_home();

			if (page != SecondScreenPanel::HOME)
				get_panel().go_to(page);

			overlay = true;

			return true;
		}

		void draw_overlay()
		{
			if (!overlay_showing())
				return;

			Point<int16_t> space = overlay_size();

			// Dim the game behind it rather than clearing to black. The player
			// is standing in a map with monsters in it, and blanking the
			// screen to open a menu loses them the one thing they might need
			// to see. Dark enough to read the panel over, light enough to
			// notice something coming.
			ColorBox(space.x(), space.y(), Color::Name::BLACK, 0.72f)
				.draw(DrawArgument(Point<int16_t>(0, 0)));

			get_panel().update();
			get_panel().draw(space);
		}

		bool overlay_send_cursor(Point<int16_t> position, bool pressed, bool released)
		{
			if (!overlay_showing())
				return false;

			get_panel().send_touch(position, overlay_size(), pressed, released);

			// Back from the top level puts it away. On the Thor's panel that
			// gesture has nowhere to go and does nothing; here it is the way
			// out, and the same swipe closes it that closed every page on the
			// way in.
			if (get_panel().take_back_at_root())
				overlay = false;

			// EVERY press while it is open belongs to it, whether or not it
			// landed on a control. The overlay covers the game completely, so
			// a press that falls between two buttons is a miss on the menu -
			// not an instruction to the character standing behind it.
			return true;
		}

		void scroll(double yoffset)
		{
			// panel_ptr rather than get_panel(): scrolling must not be the
			// thing that BUILDS the panel. On a one-screen device there is
			// nothing to scroll, and creating it here would make a phone pay
			// for a second screen it does not have.
			//
			if (!available() || !panel_ptr)
				return;

			panel_ptr->send_scroll(yoffset);
		}

		UIElement* hosted(UIElement::Type type)
		{
			// Deliberately not get_panel(): asking whether a window exists must
			// not be what brings the panel into being.
			if (!panel_ptr)
				return nullptr;

			return panel_ptr->hosted(type);
		}

		bool has_cursor()
		{
			return panel_ptr && panel_ptr->has_cursor();
		}

		Keyboard::Mapping selected_mapping()
		{
			// Same reasoning as hosted(): a question must not build a panel.
			if (!panel_ptr)
				return {};

			return panel_ptr->selected_mapping();
		}

		void clear_carried()
		{
			if (panel_ptr)
				panel_ptr->clear_carried();
		}

		Keyboard::Mapping carried_mapping()
		{
			if (!panel_ptr)
				return {};

			return panel_ptr->carried_mapping();
		}

		void show_hotkeys()
		{
			if (panel_ptr)
				panel_ptr->show_page(SecondScreenPanel::HOTKEYS);
		}

		UIElement* open_trade()
		{
			if (!available())
				return nullptr;

			return get_panel().open_trade();
		}

		UIElement* open_storage()
		{
			if (!available())
				return nullptr;

			return get_panel().open_storage();
		}

		bool show_shop(UIElement* shop, bool equipment)
		{
			if (!available() || shop == nullptr)
				return false;

			get_panel().show_guest(shop,
				equipment ? "ShopEquipBg" : "ShopItemBg");

			return true;
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
