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

#include "Cursor.h"
#include "Keyboard.h"
#include "UIState.h"

#include "Components/Icon.h"
#include "Components/Textfield.h"
#include "Components/ScrollingNotice.h"
#include "SecondScreen.h"

#include "../Template/Singleton.h"
#include "../Template/Optional.h"

#include <unordered_map>

namespace ms
{
	class UI : public Singleton<UI>
	{
	public:
		enum State
		{
			LOGIN,
			GAME,
			// The cash shop replaces the world rather than opening over it,
			// the same way the login screen does - so it is a state, not a
			// window. Entering unloads the map; leaving loads it again.
			CASHSHOP
		};

		UI();

		void init();
		void draw(float alpha) const;
		void update();

		void enable();
		void disable();
		void change_state(State state);

		void quit();
		bool not_quitted() const;

		// Whether the pointer is drawn on the main screen.
		//
		// There is one cursor between two screens, and it is wherever it was
		// last touched. Showing it on both at once would say there are two.
		void set_cursor_visible(bool visible);
		bool is_cursor_visible() const;

		// Draw the one cursor on the lower panel instead, at a point given in
		// that panel's own coordinates.
		void draw_cursor_at(Point<int16_t> pos, float alpha, Cursor::State state) const;


		void send_cursor(Point<int16_t> pos);
		void send_cursor(bool pressed);
		void send_cursor(Point<int16_t> cursorpos, Cursor::State cursorstate);
		void send_focus(int focused);
		void send_scroll(double yoffset);
		void send_close();
		void rightclick();
		void doubleclick();
		void send_key(int32_t keycode, bool pressed);

		// Text arriving already decoded rather than as a keycode. An on-screen
		// keyboard reports what the user typed, not which physical key was
		// pressed, and routing that back through the keycode map would lose
		// case - passwords are case sensitive, so it has to bypass it.
		void send_text(const std::string& text);

		// Lets the platform layer raise an on-screen keyboard only while there
		// is somewhere for the text to go.
		bool has_focused_textfield() const;

		void set_scrollnotice(const std::string& notice);
		void focus_textfield(Textfield* textfield);
		void remove_textfield();

		// Whether a window is currently taking the keys.
		//
		// The same question send_key asks before deciding where a key goes, so
		// the two cannot disagree: if this says yes, the next key WILL go to a
		// window rather than to the game.
		bool window_has_focus();

		// Hand confirm / back / deny / close to the window that has focus.
		// Does nothing if none has - the caller checks window_has_focus first.
		void send_window_action(UIElement::Action action);
		void drag_icon(Icon* icon);

		// Stop holding this icon if it is the one being dragged. Called by the
		// icon's own destructor, so the UI can never be left pointing at one
		// that has gone.
		void forget_dragged(const Icon* icon);

		void add_keymapping(uint8_t no, uint8_t type, int32_t action);

		void clear_tooltip(Tooltip::Parent parent);
		void show_equip(Tooltip::Parent parent, int16_t slot);
		void show_item(Tooltip::Parent parent, int32_t item_id);
		void show_skill(Tooltip::Parent parent, int32_t skill_id, int32_t level, int32_t masterlevel, int64_t expiration);
		void show_text(Tooltip::Parent parent, std::string text);
		void show_map(Tooltip::Parent parent, std::string name, std::string description, int32_t mapid, bool bolded);

		Keyboard& get_keyboard();
		int64_t get_uptime();
		uint16_t get_uplevel();
		int64_t get_upexp();

		template <class T, typename...Args>
		Optional<T> emplace(Args&& ...args);
		template <class T>
		Optional<T> get_element();

		// The same lookup, but WITHOUT falling through to the lower panel.
		//
		// For keyboard routing. A window on the panel is a thing being read -
		// it is operated by touch, on a screen the player is not typing at -
		// so it must never claim keys from the game. The panel's world map is
		// on screen permanently, so when it did claim them, every arrow key in
		// the game went to it and the character could not move at all.
		template <class T>
		Optional<T> get_main_element();

		void remove(UIElement::Type type);

	private:
		std::unique_ptr<UIState> state;
		Keyboard keyboard;
		Cursor cursor;
		bool cursor_visible = true;
		ScrollingNotice scrollingnotice;

		Optional<Textfield> focusedtextfield;
		std::unordered_map<int32_t, bool> is_key_down;

		bool enabled;
		bool quitted;
		bool caps_lock_enabled = false;
	};

	template <class T, typename...Args>
	Optional<T> UI::emplace(Args&& ...args)
	{
		if (auto iter = state->pre_add(T::TYPE, T::TOGGLED, T::FOCUSED))
		{
			(*iter).second = std::make_unique<T>(
				std::forward<Args>(args)...
				);
		}

		return state->get(T::TYPE);
	}

	template <class T>
	Optional<T> UI::get_element()
	{
		UIElement::Type type = T::TYPE;
		UIElement* element = state->get(type);

		// Then the lower panel. A window hosted down there is not in the
		// state's list, so without this every piece of code that keeps a
		// window up to date - the inventory handlers above all - would be
		// talking to a window that is not the one on screen.
		if (!element)
			element = SecondScreen::hosted(type);

		return static_cast<T*>(element);
	}

	template <class T>
	Optional<T> UI::get_main_element()
	{
		return static_cast<T*>(state->get(T::TYPE));
	}
}