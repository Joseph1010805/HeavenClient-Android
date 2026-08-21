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

#include "../Console.h"
#include "../Constants.h"
#include "../Configuration.h"
#include "../Timer.h"

#include "../Graphics/GraphicsGL.h"

#include <android/log.h>
#include <unistd.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "HeavenClient", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "HeavenClient", __VA_ARGS__)

namespace ms
{
	namespace
	{
		bool running = true;
		SDL_GameController* gamepad = nullptr;

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

		void present_offscreen()
		{
			if (!scene_fbo)
				return;

			glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glViewport(0, 0, panel_w, panel_h);

			glBlitFramebuffer(
				0, 0, scene_w, scene_h,
				0, 0, panel_w, panel_h,
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
			for (int i = 0; i < SDL_NumJoysticks(); ++i)
			{
				if (!SDL_IsGameController(i))
					continue;

				gamepad = SDL_GameControllerOpen(i);

				if (gamepad)
				{
					LOGI("gamepad connected: %s", SDL_GameControllerName(gamepad));
					return;
				}
			}

			LOGI("no gamepad detected at startup");
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
		bool wants_text = UI::get().has_focused_textfield();

		if (wants_text && !SDL_IsTextInputActive())
			SDL_StartTextInput();
		else if (!wants_text && SDL_IsTextInputActive())
			SDL_StopTextInput();

		SDL_Event ev;

		while (SDL_PollEvent(&ev))
		{
			switch (ev.type)
			{
			case SDL_QUIT:
				UI::get().send_close();
				running = false;
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

				if (button < SDL_CONTROLLER_BUTTON_MAX)
				{
					int16_t key = padmap[button];

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

				break;
			}

			case SDL_CONTROLLERDEVICEADDED:
				if (!gamepad)
					open_gamepad();

				break;

			case SDL_CONTROLLERDEVICEREMOVED:
				if (gamepad)
				{
					SDL_GameControllerClose(gamepad);
					gamepad = nullptr;
					LOGI("gamepad disconnected");
				}

				break;

			case SDL_FINGERDOWN:
			case SDL_FINGERMOTION:
				// Touching this screen brings the pointer back to it.
				UI::get().set_cursor_visible(true);

				UI::get().send_cursor(
					Point<int16_t>(
						static_cast<int16_t>(ev.tfinger.x * width),
						static_cast<int16_t>(ev.tfinger.y * height)
					)
				);

				if (ev.type == SDL_FINGERDOWN)
					UI::get().send_cursor(true);

				break;

			case SDL_FINGERUP:
			{
				auto diff_ms = ContinuousTimer::get().stop(click_start) / 1000;
				click_start = ContinuousTimer::get().start();

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
		bind_offscreen();
		GraphicsGL::get().clearscene();
	}

	void Window::end() const
	{
		GraphicsGL::get().flush(opacity);
		present_offscreen();
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
}

#endif // PLATFORM_ANDROID
