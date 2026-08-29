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
#include "UIMegaphone.h"

#include "../UI.h"

#include "../../Net/Packets/InventoryPackets.h"
#include "../../Speech.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UIMegaphone::UIMegaphone() : UIDragElement<PosMEGAPHONE>(Point<int16_t>(WIDTH, TITLE_H))
	{
		// The item tooltip's 9-slice, same backdrop the party panel uses. There
		// is no stock megaphone window to borrow - the original client put the
		// message in a system dialog - so the game's own frame is the closest
		// thing to a house style.
		frame = MapleFrame(nl::nx::ui["UIToolTip.img"]["Item"]["Frame2"]);

		title = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::WHITE);
		title.change_text("Megaphone");

		hint = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::LIGHTGREY);

		label = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);

		scope = Scope::CHANNEL;
		ear = false;
		listening = false;

		message = Textfield(
			Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE,
			Rectangle<int16_t>(
				Point<int16_t>(PAD, TITLE_H + 6),
				Point<int16_t>(WIDTH - PAD, TITLE_H + 6 + 14)),
			70);

		message.set_state(Textfield::State::FOCUSED);

		// Enter sends, the same as it does in the chat bar. Anyone who has
		// typed a line of chat already knows how to use this.
		message.set_enter_callback(
			[&](std::string)
			{
				fire();
			});

		layout();

		dimension = Point<int16_t>(WIDTH, TITLE_H + ROW_H * 3 + PAD);
	}

	void UIMegaphone::layout()
	{
		// One list of controls, used for both drawing and hit testing, so the
		// two cannot end up disagreeing about where a button is - which is the
		// usual way a control ends up looking pressable and doing nothing.
		int16_t row2 = TITLE_H + ROW_H;
		int16_t row3 = TITLE_H + ROW_H * 2;

		auto box = [](int16_t x, int16_t y, int16_t w, int16_t h)
			{
				return Rectangle<int16_t>(
					Point<int16_t>(x, y), Point<int16_t>(x + w, y + h));
			};

		controls[CT_CHANNEL] = { box(PAD, row2, 74, 18), "Channel", true };
		controls[CT_WORLD] = { box(PAD + 80, row2, 60, 18), "World", false };
		controls[CT_EAR] = { box(PAD + 146, row2, 52, 18), "Ear", false };

		controls[CT_MIC] = { box(PAD, row3, 60, 18), "Speak", false };
		controls[CT_SEND] = { box(WIDTH - PAD - 118, row3, 56, 18), "Send", false };
		controls[CT_CLOSE] = { box(WIDTH - PAD - 56, row3, 56, 18), "Close", false };
	}

	void UIMegaphone::draw(float inter) const
	{
		int16_t panel_h = static_cast<int16_t>(TITLE_H + ROW_H * 3 + PAD);

		frame.draw(position + Point<int16_t>(WIDTH / 2, panel_h - 6),
			WIDTH - 19, panel_h - 17);

		title.draw(position + Point<int16_t>(PAD, 2));

		hint.change_text(scope == Scope::CHANNEL
			? "Everyone on this channel"
			: "Everyone in the world");
		hint.draw(position + Point<int16_t>(PAD + 78, 4));

		message.draw(position);

		for (int i = 0; i < NUM_CONTROLS; i++)
		{
			const Hit& c = controls[i];

			// A pressed-looking control is just a brighter label. Drawing a
			// filled rectangle behind it would need a texture this window does
			// not have, and colour carries the state perfectly well.
			bool lit = c.on
				|| (i == CT_CHANNEL && scope == Scope::CHANNEL)
				|| (i == CT_WORLD && scope == Scope::WORLD)
				|| (i == CT_EAR && ear)
				|| (i == CT_MIC && listening);

			label.change_color(lit ? Color::Name::WHITE : Color::Name::LIGHTGREY);
			label.change_text(i == CT_MIC && listening ? "Listening" : c.label);

			label.draw(position + Point<int16_t>(
				static_cast<int16_t>((c.bounds.left() + c.bounds.right()) / 2),
				static_cast<int16_t>(c.bounds.top())));
		}

		UIElement::draw(inter);
	}

	void UIMegaphone::update()
	{
		message.update(position);

		// The recogniser runs on its own thread and leaves a finished phrase
		// behind it. Collected here rather than pushed from there, because
		// touching a Textfield from another thread is the sort of thing that
		// works until it does not.
		if (listening)
		{
			std::string heard = Speech::get().take_phrase();

			if (!heard.empty())
			{
				message.change_text(heard);
				listening = false;
			}
			else if (!Speech::get().is_listening())
			{
				listening = false;
			}
		}

		UIElement::update();
	}

	void UIMegaphone::fire()
	{
		std::string text = message.get_text();

		if (text.empty())
			return;

		int32_t itemid = (scope == Scope::WORLD)
			? ID_SUPER_MEGAPHONE
			: ID_MEGAPHONE;

		MegaphonePacket(itemid, text, ear).dispatch();

		message.change_text("");

		deactivate();
	}

	void UIMegaphone::set_message(const std::string& text)
	{
		message.change_text(text);
		listening = false;
	}

	bool UIMegaphone::wants_dictation() const
	{
		return active && listening;
	}

	void UIMegaphone::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			deactivate();
			return;
		}

		message.send_key(KeyType::Id::TEXT, keycode, pressed);
	}

	void UIMegaphone::send_action(Action action)
	{
		// The Quest's controllers, which have no keyboard at all: A confirms,
		// B goes back, Y closes. Handled here so the window behaves the same
		// way every other window does on that headset.
		switch (action)
		{
		case Action::CONFIRM:
			fire();
			break;
		case Action::BACK:
		case Action::DENY:
		case Action::CLOSE:
			deactivate();
			break;
		}
	}

	Cursor::State UIMegaphone::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Point<int16_t> local = cursorpos - position;

		for (int i = 0; i < NUM_CONTROLS; i++)
		{
			if (!controls[i].bounds.contains(local))
				continue;

			if (!clicked)
				return Cursor::State::CANCLICK;

			switch (i)
			{
			case CT_CHANNEL:
				scope = Scope::CHANNEL;
				break;
			case CT_WORLD:
				scope = Scope::WORLD;
				break;
			case CT_EAR:
				ear = !ear;
				break;
			case CT_MIC:
				if (listening)
				{
					Speech::get().stop();
					listening = false;
				}
				else
				{
					listening = Speech::get().start();
				}
				break;
			case CT_SEND:
				fire();
				break;
			case CT_CLOSE:
				deactivate();
				break;
			}

			return Cursor::State::CLICKING;
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	UIElement::Type UIMegaphone::get_type() const
	{
		return TYPE;
	}
}
