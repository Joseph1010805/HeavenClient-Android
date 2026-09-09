//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIAvatarMega.h"

#include "../../Constants.h"
#include "../../Graphics/GraphicsGL.h"

namespace ms
{
	namespace
	{
		// THE FIVE MESSENGERS, AS COLOURS.
		//
		// They are the same broadcast in different clothes, and the colour is
		// the only thing that tells one from another - so it is looked up from
		// the item id rather than being a field somebody has to remember to
		// set. Anything unknown falls back to the first, because a banner in
		// the wrong colour still says what it was sent to say.
		struct Skin { int32_t item; float r, g, b; };

		const Skin SKINS[] = {
			{ 5390000, 0.62f, 0.16f, 0.16f },   // Diablo
			{ 5390001, 0.18f, 0.42f, 0.70f },   // Cloud 9
			{ 5390002, 0.74f, 0.28f, 0.52f },   // Loveholic
			{ 5390005, 0.86f, 0.56f, 0.14f },   // Cute Tiger
			{ 5390006, 0.74f, 0.34f, 0.08f }    // Roaring Tiger
		};

		const Skin& skin_of(int32_t item)
		{
			for (const Skin& s : SKINS)
				if (s.item == item)
					return s;

			return SKINS[0];
		}

		constexpr int16_t HEIGHT = 74;
		constexpr int16_t AVATAR_X = 44;
		constexpr int16_t TEXT_X = 96;
	}

	namespace
	{
		// The last face this device sent, and who sent it. Static because the
		// banner that will use it does not exist yet when SEND is pressed -
		// it arrives from the server a moment later, like everybody else's.
		std::string pending_who;
		Expression::Id pending_face = Expression::Id::DEFAULT;
	}

	void UIAvatarMega::remember_face(const std::string& who, Expression::Id f)
	{
		pending_who = who;
		pending_face = f;
	}

	UIAvatarMega::UIAvatarMega(int32_t itemid, const std::string& who,
		const std::string lines[4], const LookEntry& entry,
		Expression::Id chosen)
		: UIElement(Point<int16_t>(0, 0), Point<int16_t>(0, 0)),
		item(itemid), look(entry), face(chosen)
	{
		// OUR OWN BANNER WEARS THE FACE WE PICKED.
		//
		// The name arrives decorated with any medal, so it is matched by
		// ENDING rather than equality - and only claimed once, so a second
		// banner from somebody else cannot inherit it.
		if (!pending_who.empty() && who.size() >= pending_who.size()
			&& who.compare(who.size() - pending_who.size(),
				pending_who.size(), pending_who) == 0)
		{
			face = pending_face;
			pending_who.clear();
		}

		// Facing the reader, standing still. The banner is a portrait, not a
		// scene - nothing here walks.
		look.set_direction(true);
		look.set_stance(Stance::Id::STAND1);

		name_text = Text(Text::Font::A12B, Text::Alignment::LEFT,
			Color::Name::WHITE, who);

		for (size_t i = 0; i < 4; i++)
		{
			if (lines[i].empty())
				continue;

			line_text[lines_used] = Text(Text::Font::A11M,
				Text::Alignment::LEFT, Color::Name::WHITE, lines[i]);

			lines_used++;
		}
	}

	void UIAvatarMega::draw(float) const
	{
		int16_t w = Constants::Constants::get().get_viewwidth();

		const Skin& s = skin_of(item);

		// The band, and a darker plate behind the words so a pale backdrop
		// cannot swallow them.
		GraphicsGL::get().drawrectangle(0, 0, w, HEIGHT, s.r, s.g, s.b, 0.92f);
		GraphicsGL::get().drawrectangle(
			TEXT_X - 8, 6, w - TEXT_X, HEIGHT - 12, 0.0f, 0.0f, 0.0f, 0.28f);

		// The sender, standing in the band. Drawn from the feet, which is
		// where CharLook measures from.
		// The PUBLIC four-argument draw, which takes the stance and the
		// expression outright. The other one interpolates and is private -
		// and this is a portrait, so nothing here needs interpolating.
		look.draw(Point<int16_t>(AVATAR_X, HEIGHT - 8), false,
			Stance::Id::STAND1, face);

		name_text.draw(Point<int16_t>(TEXT_X, 4));

		for (int16_t i = 0; i < lines_used; i++)
			line_text[i].draw(Point<int16_t>(
				TEXT_X, static_cast<int16_t>(22 + i * 13)));
	}

	void UIAvatarMega::update()
	{
		UIElement::update();

		look.update(Constants::TIMESTEP);

		left -= Constants::TIMESTEP;

		if (left <= 0)
			deactivate();
	}

	UIElement::Type UIAvatarMega::get_type() const
	{
		return TYPE;
	}

	void UIAvatarMega::expire()
	{
		deactivate();
	}
}
