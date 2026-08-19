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

		// Gamepad button -> GLFW key code, populated from Setting<Joystick_*>.
		// SDL_CONTROLLER_BUTTON_MAX is small, so a flat array beats a map here.
		int16_t padmap[SDL_CONTROLLER_BUTTON_MAX];

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

			// The d-pad drives movement, which the game already reads as arrows.
			padmap[SDL_CONTROLLER_BUTTON_DPAD_UP]    = GLFW_KEY_UP;
			padmap[SDL_CONTROLLER_BUTTON_DPAD_DOWN]  = GLFW_KEY_DOWN;
			padmap[SDL_CONTROLLER_BUTTON_DPAD_LEFT]  = GLFW_KEY_LEFT;
			padmap[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = GLFW_KEY_RIGHT;

			padmap[SDL_CONTROLLER_BUTTON_START] = GLFW_KEY_ESCAPE;
			padmap[SDL_CONTROLLER_BUTTON_BACK]  = GLFW_KEY_ENTER;
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
		if (glwnd)
		{
			SDL_GL_DeleteContext(context);
			SDL_DestroyWindow(glwnd);
			context = nullptr;
			glwnd = nullptr;
		}

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

		// Android decides the surface size; the client renders at its own
		// virtual resolution and SDL scales it to fit the panel.
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

		int drawable_w = width;
		int drawable_h = height;
		SDL_GL_GetDrawableSize(glwnd, &drawable_w, &drawable_h);
		glViewport(0, 0, drawable_w, drawable_h);

		LOGI("window %dx%d, drawable %dx%d, GL_VERSION %s",
			width, height, drawable_w, drawable_h, glGetString(GL_VERSION));

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
		SDL_Event ev;

		while (SDL_PollEvent(&ev))
		{
			switch (ev.type)
			{
			case SDL_QUIT:
				UI::get().send_close();
				running = false;
				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
			{
				int key = to_glfw_key(ev.key.keysym.sym);

				if (key != GLFW_KEY_UNKNOWN)
					UI::get().send_key(key, ev.type == SDL_KEYDOWN);

				break;
			}

			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
			{
				Uint8 button = ev.cbutton.button;

				if (button < SDL_CONTROLLER_BUTTON_MAX)
				{
					int16_t key = padmap[button];

					if (key != GLFW_KEY_UNKNOWN)
						UI::get().send_key(key, ev.type == SDL_CONTROLLERBUTTONDOWN);
				}

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
		GraphicsGL::get().clearscene();
	}

	void Window::end() const
	{
		GraphicsGL::get().flush(opacity);
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
