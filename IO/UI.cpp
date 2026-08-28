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
#include "UI.h"
#include "UIStateLogin.h"
#include "UIStateGame.h"
#include "UIStateCashShop.h"
#include "Window.h"

#include "../IO/UITypes/UIStatsinfo.h"
#include "../IO/UITypes/UIItemInventory.h"
#include "../IO/UITypes/UIEquipInventory.h"
#include "../IO/UITypes/UISkillbook.h"
#include "../IO/UITypes/UIQuestLog.h"
#include "../IO/UITypes/UIWorldMap.h"
#include "../IO/UITypes/UIUserList.h"
#include "../IO/UITypes/UIChatbar.h"
#include "../IO/UITypes/UIStatusbar.h"
#include "../IO/UITypes/UINotice.h"
#include "../IO/UITypes/UIShop.h"
#include "../IO/UITypes/UIChannel.h"
#include "../IO/UITypes/UIJoypad.h"
#include "../IO/UITypes/UIEvent.h"
#include "../IO/UITypes/UIChat.h"
#include "../IO/UITypes/UIKeyConfig.h"
#include "../IO/UITypes/UIOptionMenu.h"
#include "../IO/UITypes/UIQuit.h"
#include "../IO/UITypes/UINpcTalk.h"

namespace ms
{
	UI::UI()
	{
		state = std::make_unique<UIStateNull>();
		enabled = true;
	}

	void UI::init()
	{
		printf("[*} initializing cursor.\n");
		cursor.init();
		printf("[*} changing state to login.\n");
		change_state(State::LOGIN);
	}

	void UI::draw(float alpha) const
	{
		state->draw(alpha, cursor.get_position());

		// The server broadcasts a welcome the moment you log in, and it lands
		// in a bar across the top of the screen and stays there. Not drawn.
		// Restore this line to get every server notice back.
		// scrollingnotice.draw(alpha);

		if (cursor_visible)
			cursor.draw(alpha);
	}

	void UI::update()
	{
		state->update();

		scrollingnotice.update();

		cursor.update();
	}

	void UI::enable()
	{
		enabled = true;
	}

	void UI::disable()
	{
		enabled = false;
	}

	void UI::change_state(State id)
	{
		printf("====> UI::change_state\n");

		// Let go of the text field FIRST.
		//
		// focusedtextfield points at a Textfield owned by one of the elements
		// in the state we are about to replace - the character creation
		// screen's name box, most of the time. Swapping the state destroys
		// that element, and the pointer was left aimed at the wreckage.
		//
		// send_key hands every key to the focused field when there is one, so
		// the game got none of them: create a character, arrive in the world,
		// and nothing moved. Deleting a character alone never did it, because
		// the name box is only ever focused when creating one.
		//
		// It is also a use-after-free that happened not to crash, which is the
		// worse half of the bug and the reason this belongs here rather than
		// in the creation screen - every state change is entitled to assume
		// the last one let go of its own things.
		//
		// Done before the swap while the field is still alive to be told.
		remove_textfield();

		switch (id)
		{
		case State::LOGIN:
			state = std::make_unique<UIStateLogin>();
			break;
		case State::GAME:
			state = std::make_unique<UIStateGame>();
			break;
		case State::CASHSHOP:
			state = std::make_unique<UIStateCashShop>();
			break;
		}
		printf("<==== UI::change_state\n");
	}

	void UI::quit()
	{
		quitted = true;
	}

	bool UI::not_quitted() const
	{
		return !quitted;
	}

	void UI::send_cursor(Point<int16_t> cursorpos, Cursor::State cursorstate)
	{
		Cursor::State nextstate = state->send_cursor(cursorstate, cursorpos);
		cursor.set_state(nextstate);
		cursor.set_position(cursorpos);
	}

	void UI::send_focus(int focused)
	{
		if (focused)
		{
			// The window gained input focus
			uint8_t sfxvolume = Setting<SFXVolume>::get().load();
			Sound::set_sfxvolume(sfxvolume);

			uint8_t bgmvolume = Setting<BGMVolume>::get().load();
			Music::set_bgmvolume(bgmvolume);
		}
		else
		{
			// The window lost input focus
			Sound::set_sfxvolume(0);
			Music::set_bgmvolume(0);
		}
	}

	void UI::send_scroll(double yoffset)
	{
		state->send_scroll(yoffset);
	}

	void UI::send_close()
	{
		state->send_close();
	}

	void UI::send_cursor(bool pressed)
	{
		Cursor::State cursorstate = (pressed && enabled) ? Cursor::State::CLICKING : Cursor::State::IDLE;
		Point<int16_t> cursorpos = cursor.get_position();
		send_cursor(cursorpos, cursorstate);

		if (focusedtextfield && pressed)
		{
			Cursor::State tstate = focusedtextfield->send_cursor(cursorpos, pressed);

			switch (tstate)
			{
			case Cursor::State::IDLE:
				focusedtextfield = {};
				break;
			}
		}
	}

	void UI::set_cursor_visible(bool visible)
	{
		cursor_visible = visible;
	}

	bool UI::is_cursor_visible() const
	{
		return cursor_visible;
	}

	void UI::draw_cursor_at(Point<int16_t> pos, float alpha, Cursor::State state) const
	{
		cursor.draw_at(pos, alpha, state);
	}


	void UI::send_cursor(Point<int16_t> pos)
	{
		send_cursor(pos, cursor.get_state());
	}

	void UI::rightclick()
	{
		Point<int16_t> pos = cursor.get_position();
		state->rightclick(pos);
	}

	void UI::doubleclick()
	{
		Point<int16_t> pos = cursor.get_position();
		state->doubleclick(pos);
	}

	void UI::send_key(int32_t keycode, bool pressed)
	{
		if ((is_key_down[GLFW_KEY_LEFT_ALT] || is_key_down[GLFW_KEY_RIGHT_ALT]) && (is_key_down[GLFW_KEY_ENTER] || is_key_down[GLFW_KEY_KP_ENTER]))
		{
			Window::get().toggle_fullscreen();

			is_key_down[GLFW_KEY_LEFT_ALT] = false;
			is_key_down[GLFW_KEY_RIGHT_ALT] = false;
			is_key_down[GLFW_KEY_ENTER] = false;
			is_key_down[GLFW_KEY_KP_ENTER] = false;

			return;
		}

		if (is_key_down[keyboard.capslockcode()])
			caps_lock_enabled = !caps_lock_enabled;

		// WHERE DID THAT KEY GO?
		//
		// "I cannot move" has three unrelated causes that look identical from
		// the outside - a text box holding focus, a UI window claiming the
		// keys, or nothing being bound to the key at all - and guessing
		// between them has already cost two wrong fixes. So the client says
		// which one it was, once per press, rather than leaving it to be
		// inferred.
		if (pressed)
		{
			const char* who = "game";

			if (focusedtextfield)
				who = "TEXTFIELD";
			else if (auto n = get_main_element<UINpcTalk>(); n && n->is_active())
				who = "npc dialogue";
			else if (auto w = get_main_element<UIWorldMap>(); w && w->is_active())
				who = "world map";
			else if (auto s = get_main_element<UIStatusbar>(); s && s->is_menu_active())
				who = "statusbar menu";

			Keyboard::Mapping m = keyboard.get_mapping(keycode);

			Console::get().print("key " + std::to_string(keycode)
				+ " -> " + who
				+ " (type " + std::to_string(m.type)
				+ ", action " + std::to_string(m.action) + ")");
		}

		if (focusedtextfield)
		{
			bool ctrl = is_key_down[keyboard.leftctrlcode()] || is_key_down[keyboard.rightctrlcode()];

			if (ctrl)
			{
				if (!pressed)
				{
					KeyAction::Id action = keyboard.get_ctrl_action(keycode);

					switch (action)
					{
					case KeyAction::Id::COPY:
						Window::get().setclipboard(focusedtextfield->get_text());
						break;
					case KeyAction::Id::PASTE:
						focusedtextfield->add_string(Window::get().getclipboard());
						break;
					}
				}
			}
			else
			{
				bool shift = is_key_down[keyboard.leftshiftcode()] || is_key_down[keyboard.rightshiftcode()] || caps_lock_enabled;
				Keyboard::Mapping mapping = keyboard.get_text_mapping(keycode, shift);
				focusedtextfield->send_key(mapping.type, mapping.action, pressed);
			}
		}
		else
		{
			Keyboard::Mapping mapping = keyboard.get_mapping(keycode);

			bool sent = false;
			std::list<UIElement::Type> types;

			bool escape = keycode == GLFW_KEY_ESCAPE;
			bool tab = keycode == GLFW_KEY_TAB;
			bool enter = keycode == GLFW_KEY_ENTER || keycode == GLFW_KEY_KP_ENTER;
			bool up_down = keycode == GLFW_KEY_UP || keycode == GLFW_KEY_DOWN;
			bool left_right = keycode == GLFW_KEY_LEFT || keycode == GLFW_KEY_RIGHT;
			bool arrows = up_down || left_right;

			auto statusbar = UI::get().get_main_element<UIStatusbar>();
			auto channel = UI::get().get_main_element<UIChannel>();
			auto worldmap = UI::get().get_main_element<UIWorldMap>();
			auto optionmenu = UI::get().get_main_element<UIOptionMenu>();
			auto shop = UI::get().get_main_element<UIShop>();
			auto joypad = UI::get().get_main_element<UIJoypad>();
			auto rank = UI::get().get_main_element<UIRank>();
			auto quit = UI::get().get_main_element<UIQuit>();
			auto npctalk = UI::get().get_main_element<UINpcTalk>();
			//auto report = UI::get().get_main_element<UIReport>();
			//auto whisper = UI::get().get_main_element<UIWhisper>();

			if (npctalk && npctalk->is_active())
			{
				npctalk->send_key(mapping.action, pressed, escape);
				sent = true;
			}
			else if (statusbar && statusbar->is_menu_active())
			{
				statusbar->send_key(mapping.action, pressed, escape);
				sent = true;
			}
			else if (channel && channel->is_active() && mapping.action != KeyAction::Id::CHANGECHANNEL)
			{
				channel->send_key(mapping.action, pressed, escape);
				sent = true;
			}
			else if (worldmap && worldmap->is_active() && mapping.action != KeyAction::Id::WORLDMAP)
			{
				worldmap->send_key(mapping.action, pressed, escape);
				sent = true;
			}
			else if (optionmenu && optionmenu->is_active())
			{
				optionmenu->send_key(mapping.action, pressed, escape);
				sent = true;
			}
			else if (shop && shop->is_active())
			{
				shop->send_key(mapping.action, pressed, escape);
				sent = true;
			}
			else if (joypad && joypad->is_active())
			{
				joypad->send_key(mapping.action, pressed, escape);
				sent = true;
			}
			else if (rank && rank->is_active())
			{
				rank->send_key(mapping.action, pressed, escape);
				sent = true;
			}
			else if (quit && quit->is_active())
			{
				quit->send_key(mapping.action, pressed, escape);
				sent = true;
			}
			else
			{
				// All
				if (escape || tab || enter || arrows)
				{
					// Login
					types.emplace_back(UIElement::Type::WORLDSELECT);
					types.emplace_back(UIElement::Type::CHARSELECT);
					types.emplace_back(UIElement::Type::RACESELECT);			// No tab
					types.emplace_back(UIElement::Type::CLASSCREATION);			// No tab (No arrows, but shouldn't send else where)
					types.emplace_back(UIElement::Type::LOGINNOTICE);			// No tab (No arrows, but shouldn't send else where)
					types.emplace_back(UIElement::Type::LOGINNOTICE_CONFIRM);	// No tab (No arrows, but shouldn't send else where)
					types.emplace_back(UIElement::Type::LOGINWAIT);				// No tab (No arrows, but shouldn't send else where)
				}

				if (escape)
				{
					// Login
					types.emplace_back(UIElement::Type::SOFTKEYBOARD);

					// Game
					types.emplace_back(UIElement::Type::NOTICE);
					types.emplace_back(UIElement::Type::KEYCONFIG);
					types.emplace_back(UIElement::Type::CHAT);
					types.emplace_back(UIElement::Type::EVENT);
					types.emplace_back(UIElement::Type::STATSINFO);
					types.emplace_back(UIElement::Type::ITEMINVENTORY);
					types.emplace_back(UIElement::Type::EQUIPINVENTORY);
					types.emplace_back(UIElement::Type::SKILLBOOK);
					types.emplace_back(UIElement::Type::QUESTLOG);
					types.emplace_back(UIElement::Type::USERLIST);
					types.emplace_back(UIElement::Type::NPCTALK);
				}
				else if (enter)
				{
					// Login
					types.emplace_back(UIElement::Type::SOFTKEYBOARD);

					// Game
					types.emplace_back(UIElement::Type::NOTICE);
				}
				else if (tab)
				{
					// Game
					types.emplace_back(UIElement::Type::ITEMINVENTORY);
					types.emplace_back(UIElement::Type::EQUIPINVENTORY);
					types.emplace_back(UIElement::Type::SKILLBOOK);
					types.emplace_back(UIElement::Type::QUESTLOG);
					types.emplace_back(UIElement::Type::USERLIST);
				}

				/*if (types.size() > 0)
				{
					auto element = state->get_front(types);

					if (element && element != nullptr)
					{
						element->send_key(mapping.action, pressed, escape);
						sent = true;
					}
				}*/
				// TODO: (rich) fix
			}

			if (!sent)
			{
				auto chatbar = UI::get().get_element<UIChatbar>();

				if (escape)
				{
					if (chatbar && chatbar->is_chatopen())
						chatbar->send_key(mapping.action, pressed, escape);
					else
						state->send_key(mapping.type, mapping.action, pressed, escape);
				}
				else if (enter)
				{
					if (chatbar)
						chatbar->send_key(mapping.action, pressed, escape);
					else
						state->send_key(mapping.type, mapping.action, pressed, escape);
				}
				else
				{
					state->send_key(mapping.type, mapping.action, pressed, escape);
				}
			}
		}

		is_key_down[keycode] = pressed;
	}

	void UI::set_scrollnotice(const std::string& notice)
	{
		scrollingnotice.setnotice(notice);
	}

	void UI::send_text(const std::string& text)
	{
		// add_string enforces the field's own length limit, so an on-screen
		// keyboard cannot overrun a field the way send_key never could.
		if (focusedtextfield)
			focusedtextfield->add_string(text);
	}

	bool UI::has_focused_textfield() const
	{
		return focusedtextfield.operator bool();
	}

	void UI::focus_textfield(Textfield* tofocus)
	{
		if (focusedtextfield)
			focusedtextfield->set_state(Textfield::State::NORMAL);

		focusedtextfield = tofocus;
	}

	void UI::send_window_action(UIElement::Action action)
	{
		// Innermost first: a notice opened on top of a dialogue is the thing
		// being answered, and the dialogue underneath must not hear it.
		if (auto y = get_main_element<UIYesNo>(); y && y->is_active())
			return y->send_action(action);

		if (auto k = get_main_element<UIOk>(); k && k->is_active())
			return k->send_action(action);

		if (auto n = get_main_element<UINpcTalk>(); n && n->is_active())
			return n->send_action(action);

		if (auto s = get_main_element<UIShop>(); s && s->is_active())
			return s->send_action(action);

		if (auto q = get_main_element<UIQuit>(); q && q->is_active())
			return q->send_action(action);

		if (auto o = get_main_element<UIOptionMenu>(); o && o->is_active())
			return o->send_action(action);

		if (auto ch = get_main_element<UIChannel>(); ch && ch->is_active())
			return ch->send_action(action);

		if (auto w = get_main_element<UIWorldMap>(); w && w->is_active())
			return w->send_action(action);

		if (auto r = get_main_element<UIRank>(); r && r->is_active())
			return r->send_action(action);

		if (auto sb = get_main_element<UIStatusbar>(); sb && sb->is_menu_active())
			return sb->send_action(action);
	}

	bool UI::window_has_focus()
	{
		if (focusedtextfield)
			return true;

		if (auto n = get_main_element<UINpcTalk>(); n && n->is_active())
			return true;

		if (auto w = get_main_element<UIWorldMap>(); w && w->is_active())
			return true;

		if (auto s = get_main_element<UIStatusbar>(); s && s->is_menu_active())
			return true;

		if (auto c = get_main_element<UIChannel>(); c && c->is_active())
			return true;

		if (auto o = get_main_element<UIOptionMenu>(); o && o->is_active())
			return true;

		if (auto sh = get_main_element<UIShop>(); sh && sh->is_active())
			return true;

		if (auto q = get_main_element<UIQuit>(); q && q->is_active())
			return true;

		if (auto r = get_main_element<UIRank>(); r && r->is_active())
			return true;

		// The plain notices - "are you sure", "you have died" - are modal and
		// are what the confirm button most often has to answer.
		if (auto y = get_main_element<UIYesNo>(); y && y->is_active())
			return true;

		if (auto k = get_main_element<UIOk>(); k && k->is_active())
			return true;

		return false;
	}

	void UI::remove_textfield()
	{
		if (focusedtextfield)
			focusedtextfield->set_state(Textfield::State::NORMAL);

		focusedtextfield = {};
	}

	void UI::drag_icon(Icon* icon)
	{
		state->drag_icon(icon);
	}

	void UI::forget_dragged(const Icon* icon)
	{
		state->forget_dragged(icon);
	}

	void UI::add_keymapping(uint8_t no, uint8_t type, int32_t action)
	{
		keyboard.assign(no, type, action);
	}

	void UI::clear_tooltip(Tooltip::Parent parent)
	{
		state->clear_tooltip(parent);
	}

	void UI::show_equip(Tooltip::Parent parent, int16_t slot)
	{
		state->show_equip(parent, slot);
	}

	void UI::show_item(Tooltip::Parent parent, int32_t item_id)
	{
		state->show_item(parent, item_id);
	}

	void UI::show_skill(Tooltip::Parent parent, int32_t skill_id, int32_t level, int32_t masterlevel, int64_t expiration)
	{
		state->show_skill(parent, skill_id, level, masterlevel, expiration);
	}

	void UI::show_text(Tooltip::Parent parent, std::string text)
	{
		state->show_text(parent, text);
	}

	void UI::show_map(Tooltip::Parent parent, std::string name, std::string description, int32_t mapid, bool bolded)
	{
		state->show_map(parent, name, description, mapid, bolded);
	}

	Keyboard& UI::get_keyboard()
	{
		return keyboard;
	}

	int64_t UI::get_uptime()
	{
		return state->get_uptime();
	}

	uint16_t UI::get_uplevel()
	{
		return state->get_uplevel();
	}

	int64_t UI::get_upexp()
	{
		return state->get_upexp();
	}

	void UI::remove(UIElement::Type type)
	{
		focusedtextfield = {};
		state->remove(type);
	}
}