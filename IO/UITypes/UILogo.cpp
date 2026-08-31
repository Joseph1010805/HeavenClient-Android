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
				Nexon.draw(position + Point<int16_t>(440, 360), inter);
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