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

#include "../UIDragElement.h"

#include "../Components/MapleFrame.h"
#include "../Components/Textfield.h"

#include "../../Graphics/Text.h"

namespace ms
{
	// THE MEGAPHONE, AS A BUTTON RATHER THAN AN ITEM.
	//
	// Megaphones were something you bought, used once and lost. Here they are a
	// thing the game can simply do - open this, say something, and everyone on
	// the channel (or in the world) hears it.
	//
	// What goes out is not an imitation of a megaphone. It IS one: this sends
	// the ordinary USE_CASH_ITEM packet naming a real megaphone item, and the
	// server runs the same code it always did. The only thing changed on that
	// side is that it no longer insists you own one, and no longer takes it
	// away afterwards (USE_FREE_MEGAPHONES). So the artwork, the
	// "<medal> Name : " prefix, the level-10 rule and the channel-versus-world
	// scope all come from the one implementation there has ever been, and
	// cannot drift away from it.
	class UIMegaphone : public UIDragElement<PosMEGAPHONE>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MEGAPHONE;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = true;

		UIMegaphone();

		void draw(float inter) const override;
		void update() override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;
		void send_action(Action action) override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		// Hands a finished phrase from the speech recogniser to the message
		// box, so the microphone reaches this window the same way it reaches
		// the chat bar.
		void set_message(const std::string& text);

		// Whether this window is up and wants dictated text. The recogniser
		// asks rather than being told, because the window can be closed
		// between someone pressing the microphone and finishing the sentence.
		bool wants_dictation() const;

	private:
		// Item ids of the real megaphones, which is what makes this the same
		// feature rather than a copy of it. 507xxxx, and the digit that picks
		// the kind is (id / 1000) % 10 - see Cosmic's UseCashItemHandler.
		//
		//   5070000  Cheap Megaphone   -> 0   NOT a case in that switch
		//   5071000  Megaphone         -> 1   this channel
		//   5072000  Super Megaphone   -> 2   the whole world, with the ear flag
		//
		// Worth spelling out, because the obvious guess is wrong twice over:
		// the LOWEST id is not the plain megaphone, and the one that looks like
		// the plain megaphone is the cheap one, which the server ignores
		// entirely. Sending 5070000 fell through the switch and did nothing at
		// all - no shout, no error, nothing to notice.
		static constexpr int32_t ID_MEGAPHONE = 5071000;        // channel
		static constexpr int32_t ID_SUPER_MEGAPHONE = 5072000;  // whole world

		static constexpr int16_t WIDTH = 268;
		static constexpr int16_t TITLE_H = 22;
		static constexpr int16_t ROW_H = 24;
		static constexpr int16_t PAD = 10;

		enum Scope
		{
			CHANNEL,
			WORLD
		};

		// A label with a box round it that can be clicked. Cheaper than
		// hunting the stock artwork for buttons this window has no established
		// look for, and it keeps every control in one list so the hit testing
		// and the drawing cannot disagree about where anything is.
		struct Hit
		{
			Rectangle<int16_t> bounds;
			std::string label;
			bool on;
		};

		enum Control
		{
			CT_CHANNEL,
			CT_WORLD,
			CT_EAR,
			CT_MIC,
			CT_SEND,
			CT_CLOSE,
			NUM_CONTROLS
		};

		void layout();
		void fire();

		MapleFrame frame;

		Textfield message;

		Text title;

		// Both are rewritten while drawing, which is where what they should
		// say is actually known.
		mutable Text hint;
		mutable Text label;

		Hit controls[NUM_CONTROLS];

		Scope scope;
		bool ear;
		bool listening;
	};
}
