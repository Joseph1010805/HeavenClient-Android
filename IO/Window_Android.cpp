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
//
// Android implementation of ms::Window.
//
// The desktop/Switch builds use IO/Window.cpp, which is built on GLFW. GLFW has
// no Android backend, so this file provides the same class using SDL2 for the
// window, the GLES2 context and input. CMake selects one or the other; the two
// are never compiled together.
//
// Deliberately unchanged from the GLFW version:
//
//   * The keymap vocabulary is still GLFW key codes (Util/GLFWKeys.h), so
//     existing config files and IO/Keyboard.cpp work untouched.
//   * Gamepad buttons are translated to keyboard keys via Setting<Joystick_*>,
//     matching what the Switch port does inside its patched GLFW. Handling it
//     here means we do not have to fork SDL.
//
// Deliberately different:
//
//   * No glMatrixMode/glLoadIdentity - those are fixed-function GL and do not
//     exist in GLES2. The renderer is shader-based and does not rely on them.
//   * toggle_fullscreen() is a no-op; Android windows are always fullscreen.
//
#if defined(PLATFORM_ANDROID)

#include "Window.h"
#include "UI.h"
#include "UITypes/UIChatbar.h"
#include "SecondScreen.h"

#include "../Console.h"
#include "../Constants.h"
#include "../Configuration.h"
#include "../Timer.h"

#include "../Graphics/GraphicsGL.h"

#include <android/log.h>

#include <sys/stat.h>
#include <cstdio>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <unistd.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "HeavenClient", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "HeavenClient", __VA_ARGS__)

namespace ms
{
	namespace
	{
		bool running = true;
		SDL_GameController* gamepad = nullptr;

		// Everything that was opened. `gamepad` is simply the first of `pads`,
		// kept because the rest of this file already reads it as "is any
		// controller in use".
		std::vector<SDL_GameController*> pads;
		std::vector<SDL_Joystick*> sticks;

		// The panel is 1920x1080 while the client draws in a 1280x720
		// coordinate space. Rendering straight to the panel means every sprite
		// is scaled by 1.5 in the vertex shader and sampled with GL_NEAREST,
		// and a 1.5x scale cannot be expressed evenly in whole pixels: each
		// glyph row lands on one or two physical pixels depending where it
		// falls, which is what made small text unreadable rather than merely
		// soft.
		//
		// So the scene is drawn into an offscreen buffer at exactly the
		// client's resolution - pixel-for-pixel, no scaling, nearest sampling
		// is exact - and that buffer is then stretched to the panel in one
		// blit with GL_LINEAR. The upscale still happens, but it happens once,
		// on a finished image, with filtering, instead of independently per
		// sprite with none.
		GLuint scene_fbo = 0;
		GLuint scene_color = 0;

		int scene_w = 0;
		int scene_h = 0;
		int panel_w = 0;
		int panel_h = 0;

		// The window the finished scene is blitted to. Kept here because the
		// blit has to re-ask for its size and Window::glwnd is a member this
		// namespace cannot see.
		SDL_Window* present_wnd = nullptr;

		// WHERE THE PICTURE ACTUALLY LANDED ON THE PANEL.
		//
		// Once the scene is letterboxed rather than stretched, the game no
		// longer covers the whole screen - so a touch can no longer be treated
		// as a straight fraction of it. These are written by the blit and read
		// by the touch handler, which is the only way the two can agree about
		// where a pixel is.
		//
		// Getting this wrong is not subtle: every tap lands a couple of
		// hundred pixels from where it was aimed.
		int fit_x = 0;
		int fit_y = 0;
		int fit_w = 0;
		int fit_h = 0;

		// A touch, as a fraction of the panel, turned into a point in the
		// scene. The inverse of what the blit does.
		Point<int16_t> scene_point(float fx, float fy)
		{
			int16_t vw = Constants::Constants::get().get_viewwidth();
			int16_t vh = Constants::Constants::get().get_viewheight();

			// Before the first blit there is no rectangle yet, so fall back to
			// the old straight mapping rather than dividing by zero.
			if (fit_w <= 0 || fit_h <= 0 || panel_w <= 0 || panel_h <= 0)
				return Point<int16_t>(
					static_cast<int16_t>(fx * vw),
					static_cast<int16_t>(fy * vh));

			double px = static_cast<double>(fx) * panel_w - fit_x;
			double py = static_cast<double>(fy) * panel_h - fit_y;

			double sx = px * vw / fit_w;
			double sy = py * vh / fit_h;

			// A tap on a black bar is outside the game. Clamped rather than
			// dropped, so a finger a few pixels off the edge still presses the
			// button it was clearly going for.
			if (sx < 0) sx = 0;
			if (sy < 0) sy = 0;
			if (sx > vw) sx = vw;
			if (sy > vh) sy = vh;

			return Point<int16_t>(
				static_cast<int16_t>(sx),
				static_cast<int16_t>(sy));
		}

		void init_offscreen(int w, int h)
		{
			scene_w = w;
			scene_h = h;

			if (scene_fbo)
			{
				glDeleteFramebuffers(1, &scene_fbo);
				glDeleteTextures(1, &scene_color);
				scene_fbo = 0;
				scene_color = 0;
			}

			glGenTextures(1, &scene_color);
			glBindTexture(GL_TEXTURE_2D, scene_color);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

			// GL_LINEAR here is what makes the upscale smooth. It is safe in a
			// way it would not be on the sprite atlas: this texture holds one
			// complete frame, so there are no neighbouring sub-images for the
			// filter to bleed together.
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glGenFramebuffers(1, &scene_fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene_color, 0);

			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

			if (status != GL_FRAMEBUFFER_COMPLETE)
			{
				// Fall back to drawing straight to the panel. It looks worse,
				// but a client that renders badly beats one that renders
				// nothing.
				LOGE("offscreen target incomplete (0x%04X), rendering direct", status);

				glDeleteFramebuffers(1, &scene_fbo);
				glDeleteTextures(1, &scene_color);
				scene_fbo = 0;
				scene_color = 0;
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		// Keys that move the caret or change the text without producing a
		// character of their own, so they have to keep going through the
		// keycode path even while an IME is supplying the text.
		bool is_editing_key(int key)
		{
			switch (key)
			{
			case GLFW_KEY_BACKSPACE:
			case GLFW_KEY_DELETE:
			case GLFW_KEY_ENTER:
			case GLFW_KEY_KP_ENTER:
			case GLFW_KEY_TAB:
			case GLFW_KEY_ESCAPE:
			case GLFW_KEY_LEFT:
			case GLFW_KEY_RIGHT:
			case GLFW_KEY_UP:
			case GLFW_KEY_DOWN:
			case GLFW_KEY_HOME:
			case GLFW_KEY_END:
				return true;
			default:
				return false;
			}
		}

		void bind_offscreen()
		{
			if (!scene_fbo)
				return;

			glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
			glViewport(0, 0, scene_w, scene_h);
		}

		// TAKE A PICTURE OF OUR OWN FRAMEBUFFER.
		//
		// Meta sets FLAG_SECURE on the Quest's display, so `adb screencap`
		// returns a zero-byte file every time. FLAG_SECURE stops the SYSTEM
		// capturing the display; it does not stop this process reading back
		// the pixels it drew itself, which is all this does.
		//
		//     adb shell touch .../files/HeavenClient/shoot
		//     adb shell chmod a+r .../files/HeavenClient/shoot
		//
		// TWO THINGS THE FIRST VERSION GOT WRONG, both of which cost frames:
		//
		// It wrote into HeavenClient/, which adb owns and the app cannot write
		// to - so every attempt failed. And it removed the trigger to stop
		// repeating, which failed for the same reason - so it fired every 20
		// frames forever, doing a 4 MB readback and a PNG encode each time,
		// and the game crawled. A feature meant to help debugging became the
		// bug being debugged.
		//
		// Now: written to the working directory, which belongs to the app, and
		// the trigger is recognised by its TIMESTAMP rather than deleted. A
		// file we cannot remove is one we must be able to ignore.
		void maybe_screenshot()
		{
			static int countdown = 0;
			static time_t last_seen = 0;
			static int taken = 0;

			if (--countdown > 0)
				return;

			countdown = 20;

			struct stat st;

			if (stat("HeavenClient/shoot", &st) != 0)
				return;

			// Same trigger as last time - already answered.
			if (st.st_mtime == last_seen)
				return;

			last_seen = st.st_mtime;

			if (panel_w <= 0 || panel_h <= 0)
				return;

			std::vector<unsigned char> pixels(
				static_cast<size_t>(panel_w) * panel_h * 4);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glReadPixels(0, 0, panel_w, panel_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

			// GL hands back the bottom row first; every image format expects
			// the top row first.
			stbi_flip_vertically_on_write(1);

			char name[128];
			snprintf(name, sizeof(name), "shot-%02d.png", taken++ % 10);

			if (stbi_write_png(name, panel_w, panel_h, 4, pixels.data(), panel_w * 4))
				LOGI("screenshot: %s (%dx%d)", name, panel_w, panel_h);
			else
				LOGE("screenshot: could not write %s - is the working directory writable?", name);
		}

		void present_offscreen()
		{
			if (!scene_fbo)
				return;

			// Re-ask for the drawable size rather than trusting the one taken
			// at startup. Android settles rotation and window insets a beat
			// after the GL context exists, so the size measured then is
			// sometimes the pre-rotation one - and there is no
			// SDL_WINDOWEVENT handling here to notice it changing later.
			//
			// Held stale, every frame for the rest of the run is blitted to
			// the wrong rectangle: the picture is scaled to a viewport that
			// is not the screen, which reads as the game being cut off at the
			// edges. It only happens when the race falls the wrong way, which
			// is why it came and went between launches.
			int drawable_w = 0;
			int drawable_h = 0;

			if (present_wnd)
				SDL_GL_GetDrawableSize(present_wnd, &drawable_w, &drawable_h);

			if (drawable_w > 0 && drawable_h > 0)
			{
				panel_w = drawable_w;
				panel_h = drawable_h;
			}

			// SAY WHEN THE RECTANGLE CHANGES.
			//
			// The cut-off screen has now survived two fixes aimed at causes
			// that turned out not to be it - a stale drawable size, then a
			// missed rotation - and the second was disproved by its own
			// logging: the window never resized, yet the picture went crooked
			// mid-session. So rather than guess a third time, this reports the
			// actual rectangle being drawn into whenever it is not what it was
			// a frame ago. Whatever moves it will have to say so.
			static int last_sw = -1, last_sh = -1, last_pw = -1, last_ph = -1;

			if (scene_w != last_sw || scene_h != last_sh
				|| panel_w != last_pw || panel_h != last_ph)
			{
				// EVERY SIZE SDL WILL ADMIT TO, SIDE BY SIDE.
				//
				// The drawable size alone has now been trusted twice and been
				// wrong twice. The picture in the last report was NOT
				// distorted - a 4:3 scene stretched onto a 16:9 rectangle
				// would look visibly fat and it did not - so whatever is being
				// drawn into is not the rectangle this function believes in.
				//
				// The window size, the drawable size and the viewport GL
				// actually holds should all agree. Printing all three says
				// which one is lying instead of leaving it to be guessed at.
				int win_w = 0, win_h = 0;

				if (present_wnd)
					SDL_GetWindowSize(present_wnd, &win_w, &win_h);

				GLint vp[4] = { 0, 0, 0, 0 };
				glGetIntegerv(GL_VIEWPORT, vp);

				LOGI("blit: scene %dx%d -> screen %dx%d | window %dx%d | viewport %d,%d %dx%d",
					scene_w, scene_h, panel_w, panel_h,
					win_w, win_h, vp[0], vp[1], vp[2], vp[3]);

				last_sw = scene_w; last_sh = scene_h;
				last_pw = panel_w; last_ph = panel_h;
			}

			// KEEP THE SHAPE. LETTERBOX THE REST.
			//
			// This blitted 800x600 onto the whole 1920x1080 panel, which is
			// 2.4x across and 1.8x down - the game permanently a third too
			// wide, every character squat and every circle an oval. It was not
			// intermittent; it was always there, and what came and went was how
			// much ELSE went wrong on top of it.
			//
			// The scene is a fixed 800x600 on purpose - the login and character
			// screens are drawn for it and do not adapt - so the panel can only
			// be filled by distorting or by cropping. Neither is worth having.
			// Scale by whichever axis runs out first and centre what is left.
			// FILL THE PANEL. THE SHAPE IS THE PRICE.
			//
			// Letterboxing was tried and rejected at the table, for a reason
			// that beats being geometrically correct: a 4:3 scene fitted into a
			// 16:9 screen wastes 240 pixels down each side, and on a handheld
			// those pixels are what make a button big enough to hit with a
			// thumb. Stretching scales the picture 2.4x across instead of 1.8x,
			// so every control is WIDER as well as taller - which is the whole
			// point of running a 800x600 scene on a 1920x1080 panel.
			//
			// The distortion is real - a third too wide - and nobody has ever
			// noticed it in play, while the bars were noticed immediately.
			//
			// The undistorted way to fill the screen is to render at 16:9 in
			// the first place: the client already supports 1280x720 and
			// 1366x768. It is not free - UILogin draws its background at a
			// hardcoded 800x600 and the character screens are laid out for that
			// size - and it would make every control SMALLER than it is now,
			// which is the opposite of what this is for.
			fit_w = panel_w;
			fit_h = panel_h;
			fit_x = 0;
			fit_y = 0;

			int off_x = fit_x;
			int off_y = fit_y;

			glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glViewport(0, 0, panel_w, panel_h);

			// The bars have to be PAINTED. The default framebuffer holds the
			// last frame, so without this the edges keep whatever was there
			// before - which on a screen that has just been resized is a
			// smeared copy of the old picture.
			if (off_x > 0 || off_y > 0)
			{
				glDisable(GL_SCISSOR_TEST);
				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);
			}

			glBlitFramebuffer(
				0, 0, scene_w, scene_h,
				off_x, off_y, off_x + fit_w, off_y + fit_h,
				GL_COLOR_BUFFER_BIT, GL_LINEAR);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		// Gamepad button -> GLFW key code, populated from Setting<Joystick_*>.
		// SDL_CONTROLLER_BUTTON_MAX is small, so a flat array beats a map here.
		int16_t padmap[SDL_CONTROLLER_BUTTON_MAX];

		// The two triggers, which arrive as axes rather than buttons: [0] is
		// left, [1] is right. trigdown remembers whether each is currently held
		// so an axis that keeps reporting the same position does not re-fire.
		int16_t trigmap[2];
		bool trigdown[2];

		std::chrono::time_point<std::chrono::steady_clock> click_start =
			ContinuousTimer::get().start();

		// Translate an SDL key event to the GLFW key code the client expects.
		//
		// For printable ASCII the two agree apart from case: GLFW_KEY_A is 65
		// ('A') while SDLK_a is 97 ('a'), so upper-casing is the whole mapping.
		// Everything else is a small explicit table.
		int to_glfw_key(SDL_Keycode key)
		{
			if (key >= SDLK_a && key <= SDLK_z)
				return GLFW_KEY_A + (key - SDLK_a);

			if (key >= SDLK_0 && key <= SDLK_9)
				return GLFW_KEY_0 + (key - SDLK_0);

			if (key >= SDLK_F1 && key <= SDLK_F12)
				return GLFW_KEY_F1 + (key - SDLK_F1);

			switch (key)
			{
			case SDLK_SPACE:        return GLFW_KEY_SPACE;
			case SDLK_ESCAPE:       return GLFW_KEY_ESCAPE;
			case SDLK_RETURN:       return GLFW_KEY_ENTER;
			case SDLK_TAB:          return GLFW_KEY_TAB;
			case SDLK_BACKSPACE:    return GLFW_KEY_BACKSPACE;
			case SDLK_INSERT:       return GLFW_KEY_INSERT;
			case SDLK_DELETE:       return GLFW_KEY_DELETE;
			case SDLK_RIGHT:        return GLFW_KEY_RIGHT;
			case SDLK_LEFT:         return GLFW_KEY_LEFT;
			case SDLK_DOWN:         return GLFW_KEY_DOWN;
			case SDLK_UP:           return GLFW_KEY_UP;
			case SDLK_PAGEUP:       return GLFW_KEY_PAGE_UP;
			case SDLK_PAGEDOWN:     return GLFW_KEY_PAGE_DOWN;
			case SDLK_HOME:         return GLFW_KEY_HOME;
			case SDLK_END:          return GLFW_KEY_END;
			case SDLK_LSHIFT:       return GLFW_KEY_LEFT_SHIFT;
			case SDLK_RSHIFT:       return GLFW_KEY_RIGHT_SHIFT;
			case SDLK_LCTRL:        return GLFW_KEY_LEFT_CONTROL;
			case SDLK_RCTRL:        return GLFW_KEY_RIGHT_CONTROL;
			case SDLK_LALT:         return GLFW_KEY_LEFT_ALT;
			case SDLK_RALT:         return GLFW_KEY_RIGHT_ALT;
			case SDLK_MINUS:        return GLFW_KEY_MINUS;
			case SDLK_EQUALS:       return GLFW_KEY_EQUAL;
			case SDLK_LEFTBRACKET:  return GLFW_KEY_LEFT_BRACKET;
			case SDLK_RIGHTBRACKET: return GLFW_KEY_RIGHT_BRACKET;
			case SDLK_BACKSLASH:    return GLFW_KEY_BACKSLASH;
			case SDLK_SEMICOLON:    return GLFW_KEY_SEMICOLON;
			case SDLK_COMMA:        return GLFW_KEY_COMMA;
			case SDLK_PERIOD:       return GLFW_KEY_PERIOD;
			case SDLK_SLASH:        return GLFW_KEY_SLASH;
			default:                return GLFW_KEY_UNKNOWN;
			}
		}

		// The gamepad is a keyboard in disguise: each button is configured to a
		// key, and the game never learns a controller was involved. This is the
		// same contract the Switch port implements inside its patched GLFW.
		// See PadBind in Window.h.
		int32_t bind_to = 0;
		bool bind_armed = false;
		bool bind_done = false;

		// Which setting each button lives in. Only the ones a person would
		// ever rebind: the d-pad is movement and Back is quit, and offering
		// those would let somebody map away their only way out.
		bool save_pad_binding(int button, int32_t keycode)
		{
			switch (button)
			{
			case SDL_CONTROLLER_BUTTON_A:
				Setting<Joystick_A>::get().save(keycode); break;
			case SDL_CONTROLLER_BUTTON_B:
				Setting<Joystick_B>::get().save(keycode); break;
			case SDL_CONTROLLER_BUTTON_X:
				Setting<Joystick_X>::get().save(keycode); break;
			case SDL_CONTROLLER_BUTTON_Y:
				Setting<Joystick_Y>::get().save(keycode); break;
			case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
				Setting<Joystick_LB>::get().save(keycode); break;
			case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
				Setting<Joystick_RB>::get().save(keycode); break;
			case SDL_CONTROLLER_BUTTON_LEFTSTICK:
				Setting<Joystick_L3>::get().save(keycode); break;
			case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
				Setting<Joystick_R3>::get().save(keycode); break;
			case SDL_CONTROLLER_BUTTON_START:
				Setting<Joystick_START>::get().save(keycode); break;
			default:
				return false;
			}

			return true;
		}

		void load_padmap()
		{
			for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
				padmap[i] = GLFW_KEY_UNKNOWN;

			padmap[SDL_CONTROLLER_BUTTON_A]             = Setting<Joystick_A>::get().load();
			padmap[SDL_CONTROLLER_BUTTON_B]             = Setting<Joystick_B>::get().load();
			padmap[SDL_CONTROLLER_BUTTON_X]             = Setting<Joystick_X>::get().load();
			padmap[SDL_CONTROLLER_BUTTON_Y]             = Setting<Joystick_Y>::get().load();
			padmap[SDL_CONTROLLER_BUTTON_LEFTSHOULDER]  = Setting<Joystick_LB>::get().load();
			padmap[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = Setting<Joystick_RB>::get().load();
			padmap[SDL_CONTROLLER_BUTTON_LEFTSTICK]     = Setting<Joystick_L3>::get().load();
			padmap[SDL_CONTROLLER_BUTTON_RIGHTSTICK]    = Setting<Joystick_R3>::get().load();

			// The d-pad drives movement, which the game already reads as arrows.
			padmap[SDL_CONTROLLER_BUTTON_DPAD_UP]    = GLFW_KEY_UP;
			padmap[SDL_CONTROLLER_BUTTON_DPAD_DOWN]  = GLFW_KEY_DOWN;
			padmap[SDL_CONTROLLER_BUTTON_DPAD_LEFT]  = GLFW_KEY_LEFT;
			padmap[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = GLFW_KEY_RIGHT;

			padmap[SDL_CONTROLLER_BUTTON_START] = Setting<Joystick_START>::get().load();

			// Back is the quit button and is intercepted before this map is
			// read, so whatever sits here is never used.
			padmap[SDL_CONTROLLER_BUTTON_BACK] = GLFW_KEY_UNKNOWN;

			// The triggers are not buttons. SDL reports them as axes, which is
			// why L2 and R2 did nothing at all even though their settings have
			// existed all along - nothing ever read them.
			trigmap[0] = Setting<Joystick_LT>::get().load();
			trigmap[1] = Setting<Joystick_RT>::get().load();

			trigdown[0] = false;
			trigdown[1] = false;
		}

		// An analogue trigger pressed far enough counts as a button press, and
		// released far enough counts as a release. The two thresholds are
		// deliberately apart: with a single one, a trigger resting on the line
		// chatters between down and up and fires the bound skill repeatedly.
		// The left stick walks, in four directions.
		//
		// Only the triggers were read; both sticks were thrown away, so a
		// controller could press buttons and not move. That is survivable on a
		// handheld with a d-pad and fatal on a Quest, where the d-pad does not
		// exist and the stick is the only way to walk.
		//
		// Reported as arrow keys rather than as anything new: Keyboard's
		// constructor already binds the arrows to LEFT/RIGHT/UP/DOWN, so the
		// stick arrives as exactly what the game already understands, through
		// the same path as a keyboard and the same path as the d-pad.
		//
		// Two thresholds, like the triggers. A single one chatters when the
		// stick is held near it - releasing and re-pressing many times a second
		// - which reads as the character stuttering on the spot.
		//
		// Up and down matter as much as left and right here: up is how a portal
		// is entered and a rope is climbed, down is how you drop through a
		// platform.
		int stick_key[4] = { GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN };
		bool stick_down[4] = { false, false, false, false };

		// How far each stick is pushed, on its vertical axis only.
		//
		//   0 = left stick  -> top screen
		//   1 = right stick -> main screen
		// [0] LEFT stick -> the TOP screen's windows.
		// [1] RIGHT stick -> the BOTTOM panel's pages.
		//
		// Split deliberately, and each stick drives ONE screen only. An
		// earlier attempt had one stick move both, because a split was tried
		// and appeared not to work - but with both screens listening to one
		// stick there is no way to scroll one without the other, which is the
		// thing two screens need.
		Sint16 stick_scroll[2] = { 0, 0 };

		// A held stick has to keep scrolling, and a scroll is a discrete notch
		// rather than a distance - so it is repeated on a timer here rather
		// than emitted from the axis event, which only fires when the stick
		// MOVES. Holding it still would otherwise scroll once and stop.
		void pump_stick_scroll()
		{
			constexpr Sint16 DEADZONE = 9000;

			// Milliseconds between notches at the edge of the stick, and at
			// the point it starts moving. Pushing further scrolls faster,
			// which is what makes a long list bearable.
			constexpr int64_t FAST_MS = 40;
			constexpr int64_t SLOW_MS = 220;

			static int64_t due[2] = { 0, 0 };

			int64_t now = static_cast<int64_t>(SDL_GetTicks());

			for (int i = 0; i < 2; i++)
			{
				Sint16 value = stick_scroll[i];
				Sint16 magnitude = static_cast<Sint16>(value < 0 ? -value : value);

				if (magnitude < DEADZONE)
				{
					// Reset, so the next push scrolls at once instead of
					// waiting out an interval it never asked for.
					due[i] = 0;
					continue;
				}

				if (now < due[i])
					continue;

				// Full deflection is 32767. Anything past the deadzone maps
				// onto the interval, so a nudge creeps and a shove flies.
				double reach = static_cast<double>(magnitude - DEADZONE)
					/ static_cast<double>(32767 - DEADZONE);

				int64_t interval = SLOW_MS
					- static_cast<int64_t>((SLOW_MS - FAST_MS) * reach);

				due[i] = now + interval;

				// Down on the stick means down the list, which is how a wheel
				// behaves and the opposite of the axis sign.
				double notch = (value < 0) ? 1.0 : -1.0;

				// Each stick owns one screen, always - not by focus. Which
				// hand reaches which screen is the point of having two.
				if (i == 0)
					UI::get().send_scroll(notch);     // left  -> top
				else
					SecondScreen::scroll(notch);      // right -> bottom
			}
		}

		void handle_stick(int which, Sint16 value)
		{
			constexpr Sint16 PRESS = 16000;
			constexpr Sint16 RELEASE = 9000;

			// which: 0 = X axis, 1 = Y axis. Each drives an opposing pair.
			int negative = (which == 0) ? 0 : 2;   // left  / up
			int positive = (which == 0) ? 1 : 3;   // right / down

			struct { int slot; Sint16 magnitude; } side[2] = {
				{ negative, static_cast<Sint16>(value < 0 ? -value : 0) },
				{ positive, static_cast<Sint16>(value > 0 ?  value : 0) },
			};

			for (auto& s : side)
			{
				bool down = stick_down[s.slot];

				if (!down && s.magnitude >= PRESS)
					down = true;
				else if (down && s.magnitude < RELEASE)
					down = false;
				else
					continue;

				stick_down[s.slot] = down;

				UI::get().send_key(stick_key[s.slot], down);
			}
		}

		void handle_trigger(int side, Sint16 value)
		{
			constexpr Sint16 PRESS = 20000;
			constexpr Sint16 RELEASE = 12000;

			bool down = trigdown[side];

			if (!down && value >= PRESS)
				down = true;
			else if (down && value < RELEASE)
				down = false;
			else
				return;

			trigdown[side] = down;

			if (trigmap[side] != GLFW_KEY_UNKNOWN)
				UI::get().send_key(trigmap[side], down);
		}

		void open_gamepad()
		{
			// OPEN EVERY PAD, NOT JUST THE FIRST.
			//
			// The Quest lists two. One is a bluetooth pad that is usually
			// ASLEEP - dumpsys shows Enabled: false and it sends nothing until
			// somebody wiggles it. The other is the Touch controllers, which
			// present as INPUT_DEVICE_CLASS_VR_PERIPHERAL and are always live.
			//
			// Taking the first and returning meant opening the sleeping one
			// and never touching the one in the player's hands. SDL reported
			// "gamepad connected", the log looked perfectly healthy, and no
			// stick movement ever arrived - which reads exactly like a pad
			// that is not supported at all.
			//
			// Opening all of them costs nothing: events say which device they
			// came from and nothing here cares, so two pads simply both work.
			// Which is also what a second player sharing a handheld wants.
			for (int i = 0; i < SDL_NumJoysticks(); ++i)
			{
				if (SDL_IsGameController(i))
				{
					if (SDL_GameController* pad = SDL_GameControllerOpen(i))
					{
						pads.push_back(pad);

						if (!gamepad)
							gamepad = pad;

						LOGI("gamepad %d: %s", i, SDL_GameControllerName(pad));
					}
				}
				else if (SDL_Joystick* stick = SDL_JoystickOpen(i))
				{
					// SDL only calls a device a "game controller" when it
					// holds a mapping for it, keyed by GUID. A generic pad is
					// not in that database - but axis 0 and axis 1 are the
					// left stick on essentially every pad ever made, which is
					// all that walking needs.
					sticks.push_back(stick);

					LOGI("joystick %d: %s (%d axes, %d buttons) - no SDL"
						" mapping, using its axes directly", i,
						SDL_JoystickName(stick),
						SDL_JoystickNumAxes(stick),
						SDL_JoystickNumButtons(stick));
				}
			}

			if (pads.empty() && sticks.empty())
				LOGI("no gamepad detected");
		}
	}

	Window::Window()
	{
		context = nullptr;
		glwnd = nullptr;
		opacity = 1.0f;
		opcstep = 0.0f;
		width = Constants::Constants::get().get_viewwidth();
		height = Constants::Constants::get().get_viewheight();
	}

	Window::~Window()
	{
		if (gamepad)
			SDL_GameControllerClose(gamepad);

		if (context)
			SDL_GL_DeleteContext(context);

		if (glwnd)
			SDL_DestroyWindow(glwnd);

		SDL_Quit();
	}

	Error Window::init()
	{
		// Ask for the back button as a key event. Without this SDL leaves it to
		// Android, which just sends the app to the background - so the handler
		// added for it never saw anything.
		SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");

		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
		{
			LOGE("SDL_Init failed: %s", SDL_GetError());
			return Error::Code::GLFW;
		}

		load_padmap();
		open_gamepad();

		return initwindow();
	}

	Error Window::initwindow()
	{
		// Re-read the resolution rather than trusting what the constructor
		// captured. Window is a singleton built during static initialisation,
		// which runs before main() applies Width/Height from Settings, so the
		// constructor only ever sees the compiled-in defaults. Stale values
		// here also put touch input in the wrong coordinate space, because
		// finger positions are scaled by these.
		width = Constants::Constants::get().get_viewwidth();
		height = Constants::Constants::get().get_viewheight();

		if (glwnd)
		{
			SDL_GL_DeleteContext(context);
			SDL_DestroyWindow(glwnd);
			context = nullptr;
			glwnd = nullptr;
		}

		// ES3 rather than ES2 purely for glBlitFramebuffer, used to upscale the
		// offscreen target below. It is backwards compatible, so the renderer
		// and its GLSL ES 1.00 shaders are unaffected.
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

		// The size passed here is only a request: Android hands back a surface
		// the size of the panel regardless, so see the SDL_SetWindowSize call
		// below, which is what actually pins it.
		glwnd = SDL_CreateWindow(
			"MapleStory",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			width,
			height,
			SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN
		);

		if (!glwnd)
		{
			LOGE("SDL_CreateWindow failed: %s", SDL_GetError());
			return Error::Code::WINDOW;
		}

		context = SDL_GL_CreateContext(glwnd);

		if (!context)
		{
			LOGE("SDL_GL_CreateContext failed: %s", SDL_GetError());
			return Error::Code::WINDOW;
		}

		bool vsync = Setting<VSync>::get().load();
		SDL_GL_SetSwapInterval(vsync ? 1 : 0);

		if (Error error = GraphicsGL::get().init())
		{
			LOGE("GraphicsGL::init failed");
			return error;
		}

		present_wnd = glwnd;

		SDL_GL_GetDrawableSize(glwnd, &panel_w, &panel_h);
		glViewport(0, 0, width, height);

		LOGI("window %dx%d, drawable %dx%d, GL_VERSION %s",
			width, height, panel_w, panel_h, glGetString(GL_VERSION));

		init_offscreen(width, height);

		GraphicsGL::get().reinit();

		return Error::Code::NONE;
	}

	bool Window::not_closed() const
	{
		return running;
	}

	void Window::update()
	{
		updateopc();
	}

	void Window::updateopc()
	{
		if (opcstep != 0.0f)
		{
			opacity += opcstep;

			if (opacity >= 1.0f)
			{
				opacity = 1.0f;
				opcstep = 0.0f;
			}
			else if (opacity <= 0.0f)
			{
				opacity = 0.0f;
				opcstep = -opcstep;

				fadeprocedure();
			}
		}
	}

	void Window::check_events()
	{
		// There is no physical keyboard here, so text can only be entered
		// through the IME - and SDL only shows that while text input is
		// active. Tie it to whether the UI actually has a field focused: the
		// keyboard then appears for the login fields and character naming, and
		// stays out of the way the rest of the time.
		// ANDROID'S OWN KEYBOARD STAYS SHUT.
		//
		// The lower screen IS the keyboard now - see
		// SecondScreenPanel::draw_keyboard. Android's slid up over the top of
		// it, covering the thing it was meant to help with and hiding the
		// panel's own keys behind a second set that did not match them.
		//
		// SDL only raises the IME while text input is ACTIVE, so simply never
		// starting it is the whole of the fix. Nothing else changes: the
		// SDL_TEXTINPUT path still exists for a real keyboard over USB, and
		// send_text is what the panel's keys go through anyway.
		//
		// On a device with NO second screen there would be nothing to type
		// with, so the IME is still offered there.
		bool wants_text = UI::get().has_focused_textfield()
			&& !SecondScreen::available();

		if (wants_text && !SDL_IsTextInputActive())
			SDL_StartTextInput();
		else if (!wants_text && SDL_IsTextInputActive())
			SDL_StopTextInput();

		// Once a frame, before the new events land.
		pump_stick_scroll();

		SDL_Event ev;

		while (SDL_PollEvent(&ev))
		{
			switch (ev.type)
			{
			case SDL_QUIT:
				UI::get().send_close();
				running = false;
				break;

			case SDL_WINDOWEVENT:
				// THE SURFACE CHANGES SHAPE AFTER WE FIRST MEASURE IT.
				//
				// The activity is sensorLandscape and declares orientation and
				// screenSize in configChanges, so Android never recreates it -
				// it resizes the surface underneath and expects the app to
				// cope. Nothing here ever listened, so a launch that began
				// while the device was portrait left the surface 1080x1920 for
				// the entire run. SurfaceFlinger showed exactly that:
				//
				//     activeBuffer=[1080x1920]  pos=(0,55)
				//
				// against a 1920x1080 display. That is the game drawn to the
				// wrong rectangle - black down one side, the minimap and the
				// health bar over the far edge - and it came and went between
				// launches because it depends on how the device was being
				// held. Re-reading the drawable size every frame could not fix
				// it, because the surface itself was the wrong shape.
				if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED
					|| ev.window.event == SDL_WINDOWEVENT_RESIZED)
				{
					int now_w = 0;
					int now_h = 0;

					SDL_GL_GetDrawableSize(glwnd, &now_w, &now_h);

					if (now_w > 0 && now_h > 0 && (now_w != panel_w || now_h != panel_h))
					{
						LOGI("surface resized: %dx%d -> %dx%d",
							panel_w, panel_h, now_w, now_h);

						panel_w = now_w;
						panel_h = now_h;

						// The offscreen buffer is sized from the LOGICAL view,
						// not from this - begin() reconciles it against
						// Constants every frame - so only the destination
						// needs correcting here.
						glViewport(0, 0, panel_w, panel_h);
					}
				}

				break;

			case SDL_TEXTINPUT:
				// Printable characters come through here with their case
				// intact. Editing keys (backspace, enter) still arrive as
				// SDL_KEYDOWN below and go through the keycode path.
				UI::get().send_text(ev.text.text);
				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
			{
				// Android's back button, which is the only quit affordance a
				// handheld has - there is no window to close and no menu bar.
				// It opens the game's own quit dialog, so quitting still logs
				// out through the server rather than just killing the process,
				// which is what saves the character.
				if (ev.key.keysym.sym == SDLK_AC_BACK)
				{
					if (ev.type == SDL_KEYDOWN)
						UI::get().send_close();

					break;
				}

				int key = to_glfw_key(ev.key.keysym.sym);

				if (key == GLFW_KEY_UNKNOWN)
					break;

				// A keypress on the on-screen keyboard arrives twice: once as
				// SDL_TEXTINPUT with the character, and once here as a key
				// event. Both paths insert into the focused field, so every
				// letter was typed twice. While text input is active the
				// character is the IME's business - only the editing keys,
				// which produce no text, are forwarded.
				if (SDL_IsTextInputActive() && !is_editing_key(key))
					break;

				UI::get().send_key(key, ev.type == SDL_KEYDOWN);

				break;
			}

			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
			{
				Uint8 button = ev.cbutton.button;

				// The handheld's back button arrives as a controller button
				// rather than a key, which is why the SDLK_AC_BACK handler
				// never saw it. It was mapped to Enter, so pressing back
				// opened the chat box instead of offering to quit.
				//
				// send_close is what each screen already answers with its own
				// exit prompt - the "Are you ready to exit?" box on the login
				// screens, and the quit dialog in game.
				if (button == SDL_CONTROLLER_BUTTON_BACK)
				{
					if (ev.type == SDL_CONTROLLERBUTTONDOWN)
						UI::get().send_close();

					break;
				}

				// THE TWO STICK CLICKS ARE TALKING, AND ARE NOT REBINDABLE.
				//
				// A handheld has no keyboard, so the only way to say anything
				// is to speak it. Both clicks start the SAME capture - the
				// live balloon over your head, the pause that decides you have
				// finished - and differ only in how far what you said travels:
				//
				//   R3  the map you are standing on. A bubble over your head
				//       and a line in the running chat.
				//   L3  the whole world, as a banner across every screen.
				//
				// Taken before the rebinder, like Back above: these are what
				// the sticks mean, and a player who rebound them would have no
				// way left to speak at all.
				if (button == SDL_CONTROLLER_BUTTON_RIGHTSTICK
					|| button == SDL_CONTROLLER_BUTTON_LEFTSTICK)
				{
					if (ev.type == SDL_CONTROLLERBUTTONDOWN)
						if (auto chatbar = UI::get().get_element<UIChatbar>())
							chatbar->start_dictation(
								button == SDL_CONTROLLER_BUTTON_LEFTSTICK);

					break;
				}

				// BINDING? THEN THIS PRESS IS THE ANSWER, NOT AN ACTION.
				//
				// Taken before anything else looks at it, so arming the
				// capture and then pressing A binds A rather than jumping.
				if (bind_armed && ev.type == SDL_CONTROLLERBUTTONDOWN)
				{
					if (save_pad_binding(button, bind_to))
					{
						load_padmap();

						bind_armed = false;
						bind_done = true;
					}

					// A button that cannot be rebound - the d-pad, Back -
					// simply is not taken, and the player can press another.
					break;
				}

				if (button < SDL_CONTROLLER_BUTTON_MAX)
				{
					int16_t key = padmap[button];

					// WHILE A WINDOW IS OPEN, THE FACE BUTTONS ANSWER IT.
					//
					// A headset has no keyboard and no free hand, so the four
					// face buttons have to be able to work a dialogue. While
					// one is open they mean confirm / back / deny / close; the
					// moment the last window shuts they go back to whatever
					// the player mapped them to, untouched.
					//
					// Sent as ACTIONS rather than as keys, because these are
					// four different answers and a keyboard can only give two.
					// Escape has always meant "no" on the wire - byte 0 - so
					// closing a window has been declining it rather than
					// ending it, which looked identical because either way the
					// box goes away. See UIElement::Action.
					bool face = button == SDL_CONTROLLER_BUTTON_A
						|| button == SDL_CONTROLLER_BUTTON_B
						|| button == SDL_CONTROLLER_BUTTON_X
						|| button == SDL_CONTROLLER_BUTTON_Y;

					if (face && UI::get().window_has_focus())
					{
						if (ev.type == SDL_CONTROLLERBUTTONDOWN)
						{
							switch (button)
							{
							case SDL_CONTROLLER_BUTTON_A:
								UI::get().send_window_action(UIElement::Action::CONFIRM);
								break;
							case SDL_CONTROLLER_BUTTON_B:
								UI::get().send_window_action(UIElement::Action::BACK);
								break;
							case SDL_CONTROLLER_BUTTON_X:
								UI::get().send_window_action(UIElement::Action::DENY);
								break;
							case SDL_CONTROLLER_BUTTON_Y:
								UI::get().send_window_action(UIElement::Action::CLOSE);
								break;
							}
						}

						// The RELEASE has to be swallowed as well, not only the
						// press. Letting it through sends the game a key going
						// up that it never saw go down - and since a window
						// closing is exactly when the button is released, the
						// stray release would land on the game the instant it
						// got control back.
						break;
					}

					if (key != GLFW_KEY_UNKNOWN)
						UI::get().send_key(key, ev.type == SDL_CONTROLLERBUTTONDOWN);
				}

				break;
			}

			case SDL_CONTROLLERAXISMOTION:
			{
				if (ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
					handle_trigger(0, ev.caxis.value);
				else if (ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
					handle_trigger(1, ev.caxis.value);
				// THE STICKS SCROLL; THE D-PAD WALKS.
				//
				// The left stick used to walk the character, which is the
				// obvious binding on a one-screen handheld and the wrong one
				// here: the Thor has two screens and no other way to reach a
				// long window. The d-pad already sends the arrow keys (see
				// padmap), so walking lost nothing by moving there.
				//
				//   LEFT stick  -> the TOP screen's page
				//   RIGHT stick -> whatever is open on the main screen
				//
				// Which hand reaches which screen is the whole point of the
				// split, so it is worth keeping that way round.
				else if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
					stick_scroll[0] = ev.caxis.value;
				else if (ev.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
					stick_scroll[1] = ev.caxis.value;

				break;
			}

			case SDL_CONTROLLERDEVICEADDED:
			{
				// EVERY PAD THAT ARRIVES, not only the first.
				//
				// This opened one ONLY when none was open yet - so a second
				// controller paired after the game had started was seen,
				// reported by SDL, and then ignored. That is exactly the case
				// that matters here: somebody joining on the couch and pairing
				// a Bluetooth pad while the game is already running had a pad
				// that lit up and did nothing.
				//
				// Opened by the index the event carries rather than by
				// rescanning, so the ones already open are not opened twice.
				int which = ev.cdevice.which;

				if (SDL_IsGameController(which))
				{
					if (SDL_GameController* pad = SDL_GameControllerOpen(which))
					{
						pads.push_back(pad);

						if (!gamepad)
							gamepad = pad;

						LOGI("gamepad connected: %s",
							SDL_GameControllerName(pad));
					}
				}

				break;
			}

			case SDL_CONTROLLERDEVICEREMOVED:
			{
				// CLOSE THE ONE THAT LEFT, not whichever is first.
				//
				// This closed `gamepad` whatever had actually been unplugged,
				// so a guest's pad running out of battery took the HOST's
				// controller down with it - and left the closed handle in
				// `pads`, which is a dangling pointer the next time anything
				// walks that list.
				//
				// `which` is an INSTANCE id here, not a device index. They are
				// different numbers and the two events disagree about which
				// they carry, which is the usual way this gets written wrong.
				SDL_JoystickID gone = ev.cdevice.which;

				for (size_t i = 0; i < pads.size(); i++)
				{
					SDL_Joystick* stick =
						SDL_GameControllerGetJoystick(pads[i]);

					if (!stick || SDL_JoystickInstanceID(stick) != gone)
						continue;

					if (gamepad == pads[i])
						gamepad = nullptr;

					SDL_GameControllerClose(pads[i]);
					pads.erase(pads.begin() + i);

					LOGI("gamepad disconnected");

					break;
				}

				// Somebody else may still be holding one.
				if (!gamepad && !pads.empty())
					gamepad = pads.front();

				break;
			}

			case SDL_FINGERDOWN:
			case SDL_FINGERMOTION:
				// Touching this screen brings the pointer back to it.
				UI::get().set_cursor_visible(true);

				// The touch arrives as a FRACTION of the screen, so it has to be
				// multiplied by the size the game is currently drawing at - not
				// the size it was drawing at when the window was made.
				//
				// `width` and `height` are captured once at startup. The cash
				// shop draws at 1024x768 rather than 800x600, so every tap was
				// landing at 78% of the distance across, which is far enough to
				// miss the button being aimed at. Same shape of bug as the
				// renderer being left on the old size - a cached copy of
				// something that can change.
				// AND THROUGH THE LETTERBOX.
				//
				// The fraction is of the whole PANEL, but the scene no longer
				// covers the whole panel - it is centred inside it with bars
				// down the sides. Mapping the fraction straight onto the scene
				// would put every tap a couple of hundred pixels from where it
				// was aimed, worst at the edges.
				//
				// fit_x/fit_w are written by the blit, which is the only place
				// that knows where the picture landed.
			{
				Point<int16_t> at = scene_point(ev.tfinger.x, ev.tfinger.y);

				// THE ONE-SCREEN PANEL EATS THE TOUCH. It is drawn over
				// everything, so a press on it must not also reach the game
				// underneath - tapping a menu button would otherwise walk the
				// character at the same time. Returns false when it is not
				// open, which is every device that has a real second screen.
				if (SecondScreen::overlay_send_cursor(at, ev.type == SDL_FINGERDOWN, false))
					break;

				UI::get().send_cursor(at);

				if (ev.type == SDL_FINGERDOWN)
					UI::get().send_cursor(true);

				break;
			}

			case SDL_FINGERUP:
			{
				auto diff_ms = ContinuousTimer::get().stop(click_start) / 1000;
				click_start = ContinuousTimer::get().start();

				if (SecondScreen::overlay_send_cursor(
					scene_point(ev.tfinger.x, ev.tfinger.y), false, true))
					break;

				if (diff_ms > 10 && diff_ms < 200)
					UI::get().doubleclick();

				UI::get().send_cursor(false);
				break;
			}

			case SDL_APP_WILLENTERBACKGROUND:
				UI::get().send_focus(0);
				break;

			case SDL_APP_DIDENTERFOREGROUND:
				UI::get().send_focus(1);
				break;
			}
		}
	}

	void Window::begin() const
	{
		// The buffer the scene is drawn into has to match the size the scene
		// thinks it is drawing at.
		//
		// It used to be built once at startup, at 800x600, and never revisited
		// - which is right until something changes the logical view size. The
		// cash shop does exactly that, laying itself out across 1024x768; only
		// the top-left 800x600 of it was ever captured, and that was then
		// stretched over the whole screen. The result was a shop zoomed in with
		// its right-hand panel cut off.
		int16_t want_w = Constants::Constants::get().get_viewwidth();
		int16_t want_h = Constants::Constants::get().get_viewheight();

		if (want_w > 0 && want_h > 0 && (want_w != scene_w || want_h != scene_h))
			init_offscreen(want_w, want_h);

		bind_offscreen();

		// The panel draws through the same renderer and repoints it at its own
		// dimensions to do so. Reclaim them here rather than assuming whatever
		// drew last left them alone - which is what produced a main screen
		// shifted and cut off, on the launches where the panel happened to
		// come up.
		GraphicsGL::get().use_main_screen();

		GraphicsGL::get().clearscene();
	}

	void Window::end() const
	{
		GraphicsGL::get().flush(opacity);
		present_offscreen();

		// Before the swap: the default framebuffer still holds this frame.
		maybe_screenshot();

		SDL_GL_SwapWindow(glwnd);
	}

	void Window::fadeout(float step, std::function<void()> fadeproc)
	{
		opcstep = -step;
		fadeprocedure = fadeproc;
	}

	void Window::setclipboard(const std::string& text) const
	{
		SDL_SetClipboardText(text.c_str());
	}

	std::string Window::getclipboard() const
	{
		char* text = SDL_GetClipboardText();

		if (!text)
			return "";

		std::string result(text);
		SDL_free(text);

		return result;
	}

	void Window::toggle_fullscreen()
	{
		// Android surfaces are always fullscreen; nothing to toggle.
	}

	std::string Window::GetCurrentWorkingDir(void)
	{
		char buff[FILENAME_MAX];

		if (!getcwd(buff, FILENAME_MAX))
			return "";

		return std::string(buff);
	}
	namespace PadBind
	{
		void arm(int32_t keycode)
		{
			bind_to = keycode;
			bind_armed = true;
			bind_done = false;
		}

		void cancel()
		{
			bind_armed = false;
		}

		bool armed()
		{
			return bind_armed;
		}

		bool just_bound()
		{
			return bind_done;
		}

		void clear_bound()
		{
			bind_done = false;
		}
	}

}

#endif // PLATFORM_ANDROID
