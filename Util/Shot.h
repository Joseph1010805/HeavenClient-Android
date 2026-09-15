//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>

namespace ms
{
	// A PICTURE OF WHAT THEY WERE LOOKING AT, TO GO WITH THE REPORT.
	//
	// "It looked wrong" is the hardest report to act on and the easiest one to
	// make, so the report page takes the screen with it. A player who can send
	// a picture and a log has told us more than most bug trackers get.
	//
	// ⚠ IT CANNOT BE TAKEN WHERE IT IS ASKED FOR. Reading pixels needs the GL
	// context current on the surface being read, and the two screens are two
	// EGL surfaces the renderer swaps between. A capture taken from the button
	// handler would read whichever surface happened to be current - which is
	// the panel, so asking for the TOP screen would quietly hand back the
	// bottom one.
	//
	// So a request is PARKED here and collected by the render loop at the one
	// moment the wanted surface is current. One frame later than the tap, and
	// correct.
	namespace Shot
	{
		enum class Which
		{
			NONE,
			TOP,        // the game
			BOTTOM      // the panel
		};

		// Ask for a picture of one screen. Taken on the next frame that draws
		// it, and only one is ever pending.
		void request(Which which, const std::string& path);

		// Called by the renderer while `which`'s surface is current, with that
		// surface's size. Does nothing unless that is what was asked for.
		void collect(Which which, int32_t width, int32_t height);

		// Where the last picture landed, or empty if none has been taken.
		// The report names it so whoever reads the two can pair them up.
		const std::string& last_file();

		// Whether a request is still waiting, so the page can say "taking..."
		// rather than looking like the button did nothing.
		bool pending();

		// True ONCE, on the first ask after a capture reaches disk. Whoever
		// intends to attach the picture waits on this: the renderer takes it
		// a frame or more after the button, and a share armed any earlier
		// hands over a file that does not exist.
		bool take_ready();

		// Offer the report, the play log and the picture to whatever app the
		// player already has. Does nothing off Android.
		void share(const std::string& picture);

		// Copy the report and its picture into the public Downloads folder,
		// where a file manager and a USB cable can both reach them. The
		// share sheet is the nice path; this is the one that always works.
		//
		// Returns where it went, fit to show a player, or empty on failure.
		std::string save_copy(const std::string& picture);
	}
}
