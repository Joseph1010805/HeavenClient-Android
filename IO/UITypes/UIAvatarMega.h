//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../UIElement.h"

#include "../../Character/Look/CharLook.h"
#include "../../Graphics/Text.h"
#include "../../Character/Look/Face.h"

#include <string>

namespace ms
{
	// THE AVATAR MESSENGER BANNER - the "tiger megaphone".
	//
	// A strip across the top of the screen carrying the sender's CHARACTER,
	// their name and four lines of what they said. The server keeps it up for
	// ten seconds and then sends CLEAR_AVATAR_MEGAPHONE; this holds its own
	// clock as well, because a banner that never came down because one packet
	// went missing would sit over the game for ever.
	//
	// Drawn from the sender's real look, parsed by the same LoginParser the
	// map uses for every other character - so it is the same person, in the
	// same clothes, rather than a picture of one.
	class UIAvatarMega : public UIElement
	{
	public:
		// ⚠ ITS OWN TYPE, NEVER NONE - see the enum for what that cost.
		static constexpr Type TYPE = UIElement::Type::AVATARMEGA;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UIAvatarMega(int32_t itemid, const std::string& name,
			const std::string lines[4], const LookEntry& look,
			Expression::Id face);

		// ⚠ THE FACE THE SENDER PICKED, HELD FOR THEIR OWN BANNER.
		//
		// v83's SET_AVATAR_MEGAPHONE carries the sender's LOOK - gender,
		// skin, face ITEM, hair, equips - and no expression, so the face
		// cannot travel to anybody else's screen. What it can do is show on
		// the sender's own, which is where they are looking when they press
		// SEND, so the choice is remembered here and claimed by the next
		// banner that arrives under this character's name.
		static void remember_face(const std::string& who, Expression::Id face);

		void draw(float alpha) const override;
		UIElement::Type get_type() const override;
		void update() override;

		// The server's own teardown. Also called if a second banner arrives.
		void expire();

	private:
		// Which messenger it was. The five differ only in their colour, and
		// the colour is the whole of what makes it that item rather than
		// another - so it is picked from the id rather than baked in.
		int32_t item;

		mutable CharLook look;

		// Drawn every frame rather than set once: CharLook returns to its
		// default expression on its own, and a banner that blinked back to
		// neutral after a second would look like the choice had not worked.
		Expression::Id face;

		Text name_text;
		Text line_text[4];

		int16_t lines_used = 0;

		// ⚠ ITS OWN CLOCK, not only the server's. Ten seconds is what Cosmic
		// schedules; this waits twelve before giving up, so the packet wins
		// the race in the ordinary case and a lost packet still cannot leave
		// the banner up.
		int32_t left = 12000;
	};
}
