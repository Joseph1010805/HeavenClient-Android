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
#include "Configuration.h"
#include "Constants.h"
#include "Error.h"
#include "Timer.h"

#include "Audio/Audio.h"
#include "Character/Char.h"
#include "Gameplay/Stage.h"
#include "IO/UI.h"
#include "IO/Window.h"
#include "Net/Session.h"
#include "Util/NxFiles.h"
#include "Util/HardwareInfo.h"
#include "Util/ScreenResolution.h"

#include "Gameplay/Combat/DamageNumber.h"

#include <iostream>
#include <inttypes.h>

namespace ms
{
	Error init()
	{
		printf("[*] Initializing window.\n");
        if (Error error = Window::get().init()) {
			printf("[!] Error initializing window.\n");
            if (error)
                exit(error);
            //return error;
        }

		printf("[*] Initializing session.\n");
		if (Error error = Session::get().init()) {
			printf("[!] Error initializing session.\n");
            //return error;
            if (error)
                exit(error);
        }

		printf("[*] Initializing nxfiles.\n");
		if (Error error = NxFiles::init()) {
			printf("[!] Error initializing nxfiles.\n");
            if (error)
                exit(error);
            //return error;
        }

		printf("[*] Initializing sound.\n");
		if (Error error = Sound::init()) {
			printf("[!] Error initializing sound.\n");
            if (error)
                exit(error);
            //return error;
        }

		printf("[*] Initializing music.\n");
		// TODO: (rich) fix
		if (Error error = Music::init()) {
			printf("[!] Error initializing music.\n");
            if (error)
                exit(error);
            //return error;
        }

		printf("[*] Initializing Char.\n");
		Char::init();
		printf("[*] Initializing DamageNumber.\n");
		DamageNumber::init();
        printf("[*] Initializing MapPortals.\n");
		MapPortals::init();
        printf("[*] Initializing Stage.\n");
		Stage::get().init();
		printf("[*] Initializing UI.\n");
		UI::get().init();

		return Error::NONE;
	}

	void update()
	{
		Window::get().check_events();
		Window::get().update();
		Stage::get().update();
		UI::get().update();
		Session::get().read();
		Music::update_context();
	}

	void draw(float alpha)
	{
		Window::get().begin();
		Stage::get().draw(alpha);
		UI::get().draw(alpha);
		Window::get().end();
	}

	bool running()
	{
	    bool is_connected = Session::get().is_connected();
	    bool not_quitted = UI::get().not_quitted();
	    bool not_closed = Window::get().not_closed();
#if defined(__SWITCH__)
	    bool not_exiting = appletMainLoop();
#else
	    // appletMainLoop is libnx. On Android the equivalent signal arrives as
	    // SDL_QUIT, which Window::check_events turns into not_closed.
	    bool not_exiting = true;
#endif

		return not_exiting && not_quitted && not_closed;
	}

	void loop()
	{
        printf("[*] Starting timer,\n");
		Timer::get().start();
        printf("[*] Started timer,\n");
		int64_t timestep = Constants::TIMESTEP * 1000;
		int64_t accumulator = timestep;

		int64_t period = 0;
		int32_t samples = 0;

		bool show_fps = Configuration::get().get_show_fps();
        printf("[*] Starting loop,\n");
#if defined(__SWITCH__)
        appletLockExit();
#endif
        //int counter = 0;
		while (running())
		{
			int64_t elapsed = Timer::get().stop();
			//if (counter % 200 == 0)
            //    printf("[*] elapsed time: %" PRId64 "\n", elapsed);
			// Update game with constant timestep as many times as possible.
			for (accumulator += elapsed; accumulator >= timestep; accumulator -= timestep) {
                update();
            }

			// Draw the game. Interpolate to account for remaining time.
			float alpha = static_cast<float>(accumulator) / timestep;
			draw(alpha);

			if (Configuration::get().get_show_fps())
			{
				if (samples < 100)
				{
					period += elapsed;
					samples++;
				}
				else if (period)
				{
					int64_t fps = (samples * 1000000) / period;
					std::cout << "FPS: " << fps << std::endl;

					period = 0;
					samples = 0;
				}
			}
			//counter++;
		}

		Sound::close();
	}

	void start()
	{
		// Initialize and check for errors.
		if (Error error = init())
		{
			printf("[!] Error on init,\n");
			const char* message = error.get_message();
			const char* args = error.get_args();
			bool can_retry = error.can_retry();

			exit(error);
			std::cout << "Error: " << message << std::endl;

			if (args && args[0])
				std::cout << "Message: " << args << std::endl;

			if (can_retry)
				std::cout << "Enter 'retry' to try again." << std::endl;

			std::string command;
			std::cin >> command;

			if (can_retry && command == "retry")
				start();
		}
		else
		{
			loop();
		}
	}
}

#if defined(PLATFORM_ANDROID)
#include <android/log.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
	// This client reports what it is doing through printf, and on Android
	// stdout goes nowhere. Piping stdout/stderr into logcat makes the
	// existing instrumentation visible via `adb logcat` instead of having to
	// guess at failures from the outside.
	int stdio_pipe[2];

	void* stdio_logger(void*)
	{
		char buffer[512];
		ssize_t count;

		while ((count = read(stdio_pipe[0], buffer, sizeof(buffer) - 1)) > 0)
		{
			if (buffer[count - 1] == '\n')
				--count;

			buffer[count] = '\0';
			__android_log_write(ANDROID_LOG_INFO, "HeavenClient", buffer);
		}

		return nullptr;
	}

	void redirect_stdio_to_logcat()
	{
		setvbuf(stdout, nullptr, _IOLBF, 0);
		setvbuf(stderr, nullptr, _IONBF, 0);

		if (pipe(stdio_pipe) != 0)
			return;

		dup2(stdio_pipe[1], STDOUT_FILENO);
		dup2(stdio_pipe[1], STDERR_FILENO);

		pthread_t thread;

		if (pthread_create(&thread, nullptr, stdio_logger, nullptr) == 0)
			pthread_detach(thread);
	}
}

// SDL renames this to SDL_main and supplies the real entry point from its
// Java shell, which requires exactly this signature.
int main(int argc, char* argv[])
{
	redirect_stdio_to_logcat();

	// The client resolves everything - Settings, the NX data - against the
	// working directory, which on Android is "/" and is not writable. Moving
	// to the app's external files directory makes those relative paths land
	// somewhere real, and somewhere reachable over adb so data can be dropped
	// in without root:
	//   /sdcard/Android/data/org.heavenclient.android/files/
	if (const char* storage = SDL_AndroidGetExternalStoragePath())
	{
		if (chdir(storage) == 0)
		{
			printf("[*] working directory: %s\n", storage);

			// Settings live in HeavenClient/Settings relative to here. The
			// directory has to be created by the app itself: one made over
			// adb is owned by the shell user with no access for others, so
			// the app cannot even traverse into it.
			if (mkdir("HeavenClient", 0755) == 0)
				printf("[*] created HeavenClient directory\n");

			// Configuration is a singleton that load()s from its constructor,
			// which runs during static initialisation - before this chdir. At
			// that point the working directory was "/" and the settings file
			// was unreachable, so every value silently fell back to its
			// default (notably ServerIP = 127.0.0.1). Re-loading here picks
			// the file up now that the path resolves. save() was never
			// affected: it runs at exit, by which time the chdir has happened.
			ms::Configuration::get().load();
		}
		else
		{
			printf("[!] could not chdir to %s\n", storage);
		}
	}
	else
	{
		printf("[!] SDL_AndroidGetExternalStoragePath returned null\n");
	}

	ms::HardwareInfo();
	ms::ScreenResolution();
	ms::start();

	// No glfwTerminate (no GLFW) and no appletUnlockExit (libnx only).
	// Window's destructor does the SDL teardown.
	return EXIT_SUCCESS;
}
#else
int main()
{
    printf("lets start the client...\n");
	ms::HardwareInfo();
	ms::ScreenResolution();
	ms::start();

	printf("[*] Terminating GLFW...\n");
	glfwTerminate();
	printf("[*] Exiting...\n");
    appletUnlockExit();
	return EXIT_SUCCESS;
}
#endif