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
#include "UILogo.h"
#include "../../Constants.h"
#include "UILogin.h"

#include "../Configuration.h"

#include "../Audio/Audio.h"
#include "../Graphics/GraphicsGL.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UILogo::UILogo() : UIElement(Point<int16_t>(0, 0), Point<int16_t>(Constants::Constants::get().get_viewwidth(), Constants::Constants::get().get_viewheight()))
	{
		Music("BgmUI.img/NxLogo").play_once();

		nexon_ended = false;
		wizet_ended = false;
		user_clicked = false;

		nl::node Logo = nl::nx::ui["Logo.img"];

		Nexon = Logo["Nexon"];
		Wizet = Logo["Wizet"];
		WizetEnd = Logo["Wizet"]["40"];

		// The game's own wordmark, built into Map001.nx by
		// tools/make_assets.py. Absent in a checkout without that file, which
		// is why the stock artwork above is still loaded rather than replaced.
		custom_logo = nl::nx::map001["Custom"]["Logo"];
	}

	void UILogo::draw_end() const
	{
		if (!custom_logo.is_valid())
		{
			WizetEnd.draw(position + Point<int16_t>(263, 195));
			return;
		}

		// Centred on the screen rather than at Wizet's baked offset, which was
		// chosen for artwork of a different size.
		Point<int16_t> size = custom_logo.get_dimensions();

		custom_logo.draw(position + Point<int16_t>(
			(Constants::Constants::get().get_viewwidth() - size.x()) / 2,
			(Constants::Constants::get().get_viewheight() - size.y()) / 2));
	}

	void UILogo::draw(float inter) const
	{
		UIElement::draw(inter);

		if (!user_clicked)
		{
			if (!nexon_ended)
			{
				// CENTRED ON THE SCREEN, not on a number from 2005.
				//
				// This was drawn at a fixed (440, 360). The animation's frames
				// carry a centred origin, so that put the logo 40 pixels right
				// and 60 pixels BELOW the middle of an 800x600 view - measured
				// off a screenshot, not guessed: the logo's centre landed at
				// (1035, 647) on a 1920x1080 panel whose centre is (960, 540),
				// which is the same 40 and 60 once the 1.8 scale is taken out.
				//
				// The Wizet animation below keeps its own offset. Its frames
				// are a different size with a different origin, it looks
				// right where it is, and centring it too would move something
				// nobody asked to have moved.
				Nexon.draw(position + Point<int16_t>(
					static_cast<int16_t>(Constants::Constants::get().get_viewwidth() / 2),
					static_cast<int16_t>(Constants::Constants::get().get_viewheight() / 2)),
					inter);
			}
			else
			{
				if (!wizet_ended)
					Wizet.draw(position + Point<int16_t>(263, 195), inter);
				else
					draw_end();
			}
		}
		else
		{
			draw_end();
		}
	}

	void UILogo::update()
	{
		UIElement::update();

		if (!nexon_ended)
		{
			nexon_ended = Nexon.update();
		}
		else
		{
			if (!wizet_ended)
			{
				wizet_ended = Wizet.update();
			}
			else
			{
				Configuration::get().set_start_shown(true);

				// THE MOST EXPENSIVE THING IN THE GAME, AND IT IS OVER.
				//
				// Logo.img/Nexon in the v178 UI.nx is 136 frames of 720x480 -
				// 47M pixels against an atlas that holds 67M. Playing it once
				// therefore costs 70% of the texture budget for the rest of the
				// session, because the atlas evicts nothing on its own.
				//
				// Handing that room back here rather than leaving it to the
				// next screen change, because the login screen is already up by
				// then and would spend its whole life competing with an
				// animation that has finished.
				GraphicsGL::get().request_reset();

				UI::get().remove(UIElement::Type::START);
				UI::get().emplace<UILogin>();
			}
		}
	}

	Cursor::State UILogo::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Cursor::State ret = clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		if (clicked && !user_clicked)
			user_clicked = true;

		return ret;
	}

	UIElement::Type UILogo::get_type() const
	{
		return TYPE;
	}
}