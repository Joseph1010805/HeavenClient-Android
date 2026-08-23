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
#include "UIElement.h"

#include "../Audio/Audio.h"

namespace ms
{
	UIElement::UIElement(Point<int16_t> p, Point<int16_t> d, bool a) : position(p), dimension(d), active(a) {}
	UIElement::UIElement(Point<int16_t> p, Point<int16_t> d) : UIElement(p, d, true) {}
	UIElement::UIElement() : UIElement(Point<int16_t>(), Point<int16_t>()) {}

	void UIElement::draw(float alpha) const
	{
		draw_sprites(alpha);
		draw_buttons(alpha);
	}

	void UIElement::draw_sprites(float alpha) const
	{
		for (const Sprite& sprite : sprites)
			sprite.draw(position, alpha);
	}

	void UIElement::draw_sprites(float alpha, float opacity) const
	{
		for (const Sprite& sprite : sprites)
			sprite.draw(position, alpha, opacity);
	}

	void UIElement::draw_buttons(float) const
	{
		for (auto& iter : buttons)
			if (const Button* button = iter.second.get())
				button->draw(position);
	}

	void UIElement::update()
	{
		for (auto& sprite : sprites)
			sprite.update();

		for (auto& iter : buttons)
			if (Button* button = iter.second.get())
				button->update();
	}

	void UIElement::makeactive()
	{
		active = true;
	}

	void UIElement::deactivate()
	{
		active = false;
	}

	bool UIElement::is_active() const
	{
		return active;
	}

	void UIElement::toggle_active()
	{
		if (active)
			deactivate();
		else
			makeactive();
	}

	bool UIElement::is_in_range(Point<int16_t> cursorpos) const
	{
		auto bounds = Rectangle<int16_t>(position, position + dimension);

		return bounds.contains(cursorpos);
	}

	void UIElement::remove_cursor()
	{
		for (auto& btit : buttons)
		{
			auto button = btit.second.get();

			if (button->get_state() == Button::State::MOUSEOVER)
				button->set_state(Button::State::NORMAL);
		}
	}

	Cursor::State UIElement::send_cursor(bool down, Point<int16_t> pos)
	{
		Cursor::State ret = down ? Cursor::State::CLICKING : Cursor::State::IDLE;

		// Only the FIRST button under the cursor reacts. Buttons whose bounds
		// overlap used to all match, so the click sound played once per
		// overlapping button and button_pressed ran more than once for a
		// single press.
		Button* hit = nullptr;
		uint16_t hitid = 0;

		for (auto& btit : buttons)
		{
			if (btit.second->is_active() && btit.second->bounds(position).contains(pos))
			{
				hit = btit.second.get();
				hitid = btit.first;
				break;
			}
		}

		for (auto& btit : buttons)
			if (btit.second.get() != hit && btit.second->get_state() == Button::State::MOUSEOVER)
				btit.second->set_state(Button::State::NORMAL);

		if (hit)
		{
			if (down)
			{
				// Act on the press itself rather than requiring the button to
				// already be hovered. A mouse hovers before it clicks, so the
				// old two-step read naturally there; a finger does not, and
				// arrives with the button still NORMAL.
				Sound(Sound::Name::BUTTONCLICK).play();

				Button::State after = button_pressed(hitid);

				// The cursor has not gone anywhere, so settle on MOUSEOVER.
				// Dropping to NORMAL means the matching release sees a fresh
				// hover and plays the hover sound again.
				hit->set_state(after == Button::State::NORMAL
					? Button::State::MOUSEOVER : after);

				ret = Cursor::State::IDLE;
			}
			else
			{
				if (hit->get_state() == Button::State::NORMAL)
					Sound(Sound::Name::BUTTONOVER).play();

				hit->set_state(Button::State::MOUSEOVER);
				ret = Cursor::State::CANCLICK;
			}
		}

		return ret;
	}

}