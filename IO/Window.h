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
#pragma once

#include "../Error.h"

#include "../Template/Singleton.h"

#if defined(PLATFORM_ANDROID)
// Android has no GLFW backend, so SDL2 provides the window, the GLES2
// context and the input events. The GLFW_* key values are still the
// client's keymap vocabulary - see Util/GLFWKeys.h - so configs written
// against them keep working unchanged.
#include <SDL.h>
#include <GLES2/gl2.h>
#include "../Util/GLFWKeys.h"
#else
//#include <GL/glew.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#endif
#include <string>
#include <functional>

#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#elif __linux__
#include <unistd.h>
#define GetCurrentDir getcwd
//#include <GL/gl.h>
#else
#include <switch.h>
//#define GL_PROJECTION 0x1701
//#define glMatrixMode qglMatrixMode
#include <unistd.h>
#define GetCurrentDir getcwd
//#include <GL/gl.h>
//#include <EGL/egl.h>    // EGL library
//#include <EGL/eglext.h> // EGL extensions
#endif
#include<iostream>

namespace ms
{
	class Window : public Singleton<Window>
	{
	public:
		Window();
		~Window();

		Error init();
		Error initwindow();

		bool not_closed() const;
		void update();
		void begin() const;
		void end() const;
		void fadeout(float step, std::function<void()> fadeprocedure);
		void check_events();

		void setclipboard(const std::string& text) const;
		std::string getclipboard() const;

		void toggle_fullscreen();

	private:
		void updateopc();

#if defined(PLATFORM_ANDROID)
		SDL_Window* glwnd;
		SDL_GLContext context;
#else
		GLFWwindow* glwnd;
		GLFWwindow* context;
#endif
		bool fullscreen;
		float opacity;
		float opcstep;
		std::function<void()> fadeprocedure;
		int16_t width;
		int16_t height;
        std::string GetCurrentWorkingDir(void);
	};
}