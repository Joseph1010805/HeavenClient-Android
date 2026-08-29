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

#include "../UIElement.h"

#include "../Graphics/Text.h"
#include "../Components/Slider.h"

namespace ms
{
	class UINpcTalk : public UIElement
	{
	public:
		// WHICH BUTTONS TO DRAW - not what the server sent.
		//
		// These used to be wire values, and they were wrong. The server
		// (NPCConversationManager) actually sends:
		//
		//     0   say      - and TWO TRAILING BYTES say whether prev and next
		//                    are offered, which is how ok / next / prev /
		//                    nextprev are told apart. They are not separate
		//                    message types at all.
		//     1   yes-no
		//     4   simple   - a menu of choices
		//     12  accept-decline
		//
		// The old enum had 4 as accept-decline and no 12 whatsoever, so a
		// quest that asked you to accept it fell past the end of the enum,
		// became NONE, and drew with nothing on it but End Chat. That is a
		// character that cannot start its own first quest.
		//
		// The raw value still has to go back to the server untouched when the
		// player answers, so it is kept separately in `wire_type` - renumber
		// these freely, they are ours.
		enum TalkType : int8_t
		{
			NONE = -1,
			SENDOK,
			SENDYESNO,
			SENDNEXT,
			SENDPREV,
			SENDNEXTPREV,
			SENDACCEPTDECLINE,
			SENDGETTEXT,
			SENDGETNUMBER,
			SENDSIMPLE,
			LENGTH
		};

		static constexpr Type TYPE = UIElement::Type::NPCTALK;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UINpcTalk();

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		// Scroll wheel, and the thumbstick that now behaves like one.
		//
		// Slider::send_scroll has always existed; this window simply never
		// offered it anything, so the bar could only be dragged.
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		void change_text(int32_t npcid, int8_t msgtype, int16_t style, int8_t speaker, const std::string& text);

		// The one window where confirm, back, deny and close are four separate
		// answers rather than two. See UIElement::Action.
		void send_action(Action action) override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		// msgtype plus the trailing style bytes -> which buttons to draw.
		TalkType layout_for(int8_t msgtype, int16_t style);
		std::string format_text(const std::string& tx, const int32_t& npcid);

		// One choice offered by a SENDSIMPLE dialog. The server writes these
		// into the message as `#L<index>#the wording#l`, so they arrive as
		// part of the prose and have to be cut back out of it.
		struct Selection
		{
			int32_t index = 0;
			Text label;
			// Where the row was last drawn, so the cursor can be tested
			// against it. Refreshed on every draw.
			mutable Rectangle<int16_t> bounds;
		};

		// Lays the choices out under the body text and returns where they end.
		// `clip` is the vertical span of the box interior - rows outside it are
		// not drawn and are not made clickable, which is what keeps a long menu
		// inside its own frame.
		int16_t draw_selections(Point<int16_t> at, Range<int16_t> clip) const;

		// How tall the choices are altogether. Measured rather than guessed,
		// because it has to agree exactly with what draw_selections lays out -
		// if the two disagree the box is the wrong size, which is the bug this
		// was written for.
		int16_t selections_height() const;

		// Text plus choices. The box used to be sized from the TEXT alone and
		// the choices simply drawn underneath it, so a menu of eighteen
		// questions ran off the bottom of the frame, over the minimap, the HP
		// bar and the edge of the screen.
		int16_t content_height;

		// Pixels the body is scrolled up by, and how far one notch of the
		// slider moves it.
		int16_t scroll;
		static constexpr int16_t SCROLL_STEP = 16;

		mutable std::vector<Selection> selections;
		int32_t hovered_selection;

		static constexpr int16_t MAX_HEIGHT = 248;

		enum Buttons
		{
			ALLLEVEL,
			CLOSE,
			MYLEVEL,
			NEXT,
			NO,
			OK,
			PREV,
			QAFTER,
			QCNO,
			QCYES,
			QGIVEUP,
			QNO,
			QSTART,
			QYES,
			YES
		};

		Texture top;
		Texture fill;
		Texture bottom;
		Texture nametag;
		Texture speaker;

		Text text;
		Text name;

		int16_t height;
		int16_t offset;
		int16_t unitrows;
		int16_t rowmax;
		int16_t min_height;

		bool show_slider;
		bool draw_text;
		Slider slider;
		TalkType type;

		// Exactly what the server sent, to be handed back to it verbatim when
		// the player answers. NpcTalkMorePacket writes this straight onto the
		// wire, so it must never be an internal enum value.
		int8_t wire_type = 0;
		std::string formatted_text;
		size_t formatted_text_pos;
		uint16_t timestep;

		std::function<void(bool)> onmoved;
	};
}