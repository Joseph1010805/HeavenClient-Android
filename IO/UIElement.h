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

#include "KeyAction.h"
#include "Components/Button.h"
#include "Components/Icon.h"

#include "../Graphics/Sprite.h"

namespace ms
{
	// Base class for all types of user interfaces on screen.
	class UIElement
	{
	public:
		using UPtr = std::unique_ptr<UIElement>;

		enum Type
		{
			NONE,
			START,
			LOGIN,
			TOS,
			GENDER,
			WORLDSELECT,
			REGION,
			CHARSELECT,
			LOGINWAIT,
			RACESELECT,
			CLASSCREATION,
			SOFTKEYBOARD,
			LOGINNOTICE,
			LOGINNOTICE_CONFIRM,
			STATUSMESSENGER,
			STATUSBAR,
			CHATBAR,
			BUFFLIST,
			PARTYHUD,
			CASHSHOP,
			NOTICE,
			NPCTALK,
			SHOP,
			STATSINFO,
			ITEMINVENTORY,
			EQUIPINVENTORY,
			SKILLBOOK,
			QUESTLOG,
			WORLDMAP,
			USERLIST,
			MINIMAP,
			CHANNEL,
			CHAT,
			CHATRANK,
			JOYPAD,
			EVENT,
			KEYCONFIG,
			OPTIONMENU,
			QUIT,
			NUM_TYPES
		};

		virtual ~UIElement() {}

		virtual void draw(float inter) const;
		virtual void update();
		virtual void update_screen(int16_t new_width, int16_t new_height) {}

		void makeactive();
		void deactivate();
		bool is_active() const;

		virtual void toggle_active();
		virtual Button::State button_pressed(uint16_t buttonid) { return Button::State::DISABLED; }
		virtual bool send_icon(const Icon& icon, Point<int16_t> cursorpos) { return true; }

		virtual void doubleclick(Point<int16_t> cursorpos) {}
		virtual void rightclick(Point<int16_t> cursorpos) {}
		virtual bool is_in_range(Point<int16_t> cursorpos) const;
		virtual void remove_cursor();
		virtual Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos);
		virtual void send_scroll(double yoffset) {}
		virtual void send_key(int32_t keycode, bool pressed, bool escape) {}

		// What a gamepad's face buttons mean to an open window.
		//
		// A keyboard has only two answers to give a dialogue - Enter and
		// Escape - and that is all this client ever asked for. The protocol
		// has THREE, and they are genuinely different things:
		//
		//   CONFIRM   yes, next, ok, accept          NpcTalkMore  1
		//   DENY      no, decline                    NpcTalkMore  0
		//   BACK      the previous page              NpcTalkMore  0
		//   CLOSE     end the conversation entirely  NpcTalkMore -1
		//
		// Escape has always sent 0, which is DENY - so closing a window with
		// the keyboard has been answering "no" to it rather than ending it,
		// and the difference was invisible because both make the box go away.
		//
		// The default keeps that behaviour for every window that has not been
		// taught the difference; UINpcTalk overrides it, because it is the one
		// where all four mean something distinct.
		enum class Action
		{
			CONFIRM,
			BACK,
			DENY,
			CLOSE
		};

		virtual void send_action(Action action)
		{
			// Enter for yes, Escape for everything else - what a keyboard can
			// say, which is what these windows already understand.
			bool yes = action == Action::CONFIRM;

			send_key(yes ? KeyAction::Id::RETURN : KeyAction::Id::ESCAPE, true, !yes);
		}

		virtual UIElement::Type get_type() const = 0;

		// Where this is drawn, and how big it is.
		//
		// The lower panel places its pages itself: a window shown there is
		// centred in the room available rather than keeping wherever it was last
		// dragged to on the main screen.
		void set_position(Point<int16_t> pos) { position = pos; }
		Point<int16_t> get_position() const { return position; }
		Point<int16_t> get_dimension() const { return dimension; }

		// What this window currently has picked out, expressed as something a
		// key could be bound to - or NONE if nothing is selected, or the
		// selection is not the sort of thing that goes on a hotkey.
		//
		// The panel selects rather than drags, so an item chosen down there
		// cannot be carried to the quickslot bar the way the mouse carries one.
		// The bar asks instead.
		virtual Keyboard::Mapping selected_mapping() const { return {}; }

		// Whether an active button sits under this point.
		//
		// The lower panel needs to tell a button apart from the rest of a
		// window: on the world map a tap on a PLACE reads it first and travels
		// on the second tap, deliberately, so a name can be read without being
		// dragged somewhere. A button has no such excuse - pressing Back twice
		// to go back once is just wrong.
		bool button_at(Point<int16_t> pos) const
		{
			for (auto& entry : buttons)
				if (entry.second->is_active() && entry.second->bounds(position).contains(pos))
					return true;

			return false;
		}

	protected:
		UIElement(Point<int16_t> position, Point<int16_t> dimension, bool active);
		UIElement(Point<int16_t> position, Point<int16_t> dimension);
		UIElement();

		void draw_sprites(float alpha) const;

		// The window's own background art, drawn faintly. Used by the pages on
		// the lower panel, which sit on a picture of their own.
		void draw_sprites(float alpha, float opacity) const;
		void draw_buttons(float alpha) const;

		std::map<uint16_t, std::unique_ptr<Button>> buttons;
		std::vector<Sprite> sprites;
		Point<int16_t> position;
		Point<int16_t> dimension;
		bool active;
	};
}