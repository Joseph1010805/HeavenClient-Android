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
#include "UIChatbar.h"

#include "../Components/MapleButton.h"

#include "../UI.h"
#include "UIMegaphone.h"

#include "../../Speech.h"
#include "../../Audio/Audio.h"

#include "../Net/Packets/MessagingPackets.h"
#include "../Net/Packets/GameplayPackets.h"

#include "../../Gameplay/Stage.h"

#include <nlnx/nx.hpp>

#include <sstream>

namespace ms
{
	namespace
	{
		std::string lowercase(std::string s)
		{
			for (char& c : s)
				if (c >= 'A' && c <= 'Z')
					c = c - 'A' + 'a';

			return s;
		}

		std::string trim(const std::string& s)
		{
			size_t first = s.find_first_not_of(' ');

			if (first == std::string::npos)
				return "";

			return s.substr(first, s.find_last_not_of(' ') - first + 1);
		}

		// Finds a party member by name, case-insensitively. Returns 0 when
		// there is no such member, which every caller treats as "say how".
		int32_t party_member_id(const std::string& name)
		{
			std::string wanted = lowercase(trim(name));

			if (wanted.empty())
				return 0;

			for (const auto& member : Stage::get().get_player().get_party().get_members())
				if (lowercase(member.name) == wanted)
					return member.cid;

			return 0;
		}
	}

	// Handles a typed party command. Returns false if the line is not one,
	// in which case it goes to the server as ordinary chat.
	//
	// The real client has a party window for this. Until there is one, these
	// are the only way to form a party at all.
	bool UIChatbar::handle_command(const std::string& line)
	{
		if (line.empty() || line[0] != '/')
			return false;

		std::istringstream stream(line);
		std::string command;
		stream >> command;
		command = lowercase(command);

		std::string rest;
		std::getline(stream, rest);
		rest = trim(rest);

		// /w <name> <message>, and /r to answer whoever spoke last.
		if (command == "/w" || command == "/whisper")
		{
			std::istringstream who(rest);
			std::string target;
			who >> target;

			std::string body;
			std::getline(who, body);
			body = trim(body);

			if (target.empty() || body.empty())
			{
				send_chatline("[Whisper] Usage: /w <name> <message>", LineType::YELLOW);
			}
			else
			{
				WhisperPacket(target, body).dispatch();
				send_chatline("[to " + target + "] " + body, LineType::YELLOW);
				last_whisperer = target;
			}

			return true;
		}

		if (command == "/r" || command == "/reply")
		{
			if (last_whisperer.empty())
				send_chatline("[Whisper] Nobody has whispered you yet.", LineType::YELLOW);
			else if (rest.empty())
				send_chatline("[Whisper] Usage: /r <message>", LineType::YELLOW);
			else
			{
				WhisperPacket(last_whisperer, rest).dispatch();
				send_chatline("[to " + last_whisperer + "] " + rest, LineType::YELLOW);
			}

			return true;
		}

		// /invite <name> is what the original client accepted, so keep it.
		if (command == "/invite")
		{
			if (rest.empty())
				send_chatline("[Party] Usage: /invite <name>", LineType::YELLOW);
			else
			{
				InviteToPartyPacket(rest).dispatch();
				send_chatline("[Party] Invited " + rest + ".", LineType::YELLOW);
			}

			return true;
		}

		if (command != "/party" && command != "/p")
			return false;

		std::istringstream args(rest);
		std::string action;
		args >> action;
		action = lowercase(action);

		std::string argument;
		std::getline(args, argument);
		argument = trim(argument);

		const Party& party = Stage::get().get_player().get_party();

		if (action == "create")
		{
			CreatePartyPacket().dispatch();
		}
		else if (action == "leave")
		{
			LeavePartyPacket().dispatch();
		}
		else if (action == "invite")
		{
			if (argument.empty())
				send_chatline("[Party] Usage: /party invite <name>", LineType::YELLOW);
			else
			{
				InviteToPartyPacket(argument).dispatch();
				send_chatline("[Party] Invited " + argument + ".", LineType::YELLOW);
			}
		}
		else if (action == "expel" || action == "kick")
		{
			int32_t cid = party_member_id(argument);

			if (cid == 0)
				send_chatline("[Party] Usage: /party expel <name>", LineType::YELLOW);
			else
				ExpelFromPartyPacket(cid).dispatch();
		}
		else if (action == "leader")
		{
			int32_t cid = party_member_id(argument);

			if (cid == 0)
				send_chatline("[Party] Usage: /party leader <name>", LineType::YELLOW);
			else
				ChangePartyLeaderPacket(cid).dispatch();
		}
		else if (action == "list")
		{
			if (!party.is_in_party())
				send_chatline("[Party] You are not in a party.", LineType::YELLOW);
			else
				for (const auto& member : party.get_members())
					send_chatline(
						"[Party] " + member.name
						+ (member.cid == party.get_leader() ? " (leader)" : "")
						+ (member.online ? "" : " - offline"),
						LineType::YELLOW
					);
		}
		else
		{
			send_chatline("[Party] /party create | leave | invite <name>", LineType::YELLOW);
			send_chatline("[Party] /party expel <name> | leader <name> | list", LineType::YELLOW);
		}

		return true;
	}

	UIChatbar::UIChatbar() : UIDragElement<PosCHAT>(Point<int16_t>(410, -5))
	{
		chatopen = Setting<Chatopen>::get().load();
		chatopen_persist = chatopen;
		chatfieldopen = false;
		listening = false;
		dictation_quiet = 0;
		chatrows = 5;
		lastpos = 0;
		rowpos = 0;
		rowmax = -1;

		nl::node chat = nl::nx::ui["StatusBar3.img"]["chat"];
		nl::node ingame = chat["ingame"];
		nl::node view = ingame["view"];
		nl::node input = ingame["input"];
		nl::node chatTarget = chat["common"]["chatTarget"];

		chatspace[0] = view["min"]["top"];
		chatspace[1] = view["min"]["center"];
		chatspace[2] = view["min"]["bottom"];
		chatspace[3] = view["drag"];

		int16_t chattop_y = getchattop(true) - 33;
		closechat = Point<int16_t>(387, 21);

		buttons[Buttons::BT_OPENCHAT] = std::make_unique<MapleButton>(view["btMax"], Point<int16_t>(391, -7));
		buttons[Buttons::BT_CLOSECHAT] = std::make_unique<MapleButton>(view["btMin"], closechat + Point<int16_t>(0, chattop_y));
		buttons[Buttons::BT_CHAT] = std::make_unique<MapleButton>(input["button:chat"], Point<int16_t>(344, -8));
		buttons[Buttons::BT_LINK] = std::make_unique<MapleButton>(input["button:itemLink"], Point<int16_t>(365, -8));
		buttons[Buttons::BT_HELP] = std::make_unique<MapleButton>(input["button:help"], Point<int16_t>(386, -8));

		buttons[chatopen ? Buttons::BT_OPENCHAT : Buttons::BT_CLOSECHAT]->set_active(false);
		buttons[Buttons::BT_CHAT]->set_active(chatopen ? true : false);
		buttons[Buttons::BT_LINK]->set_active(chatopen ? true : false);
		buttons[Buttons::BT_HELP]->set_active(chatopen ? true : false);

		chattab_x = 6;
		chattab_y = chattop_y;
		chattab_span = 54;

		for (size_t i = 0; i < ChatTab::NUM_CHATTAB; i++)
		{
			buttons[Buttons::BT_TAB_0 + i] = std::make_unique<MapleButton>(view["tab"], Point<int16_t>(chattab_x + (i * chattab_span), chattab_y));
			buttons[Buttons::BT_TAB_0 + i]->set_active(chatopen ? true : false);
			chattab_text[ChatTab::CHT_ALL + i] = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::DUSTYGRAY, ChatTabText[i]);
		}

		chattab_text[ChatTab::CHT_ALL].change_color(Color::Name::WHITE);

		buttons[Buttons::BT_TAB_0 + ChatTab::NUM_CHATTAB] = std::make_unique<MapleButton>(view["btAddTab"], Point<int16_t>(chattab_x + (ChatTab::NUM_CHATTAB * chattab_span), chattab_y));
		buttons[Buttons::BT_TAB_0 + ChatTab::NUM_CHATTAB]->set_active(chatopen ? true : false);

		buttons[Buttons::BT_CHAT_TARGET] = std::make_unique<MapleButton>(chatTarget["all"], Point<int16_t>(5, -8));
		buttons[Buttons::BT_CHAT_TARGET]->set_active(chatopen ? true : false);

		// The megaphone, left of the three that were already here. BtChat's
		// artwork because there is no megaphone button in the stock UI - the
		// original client never needed one, since a megaphone was an item you
		// double-clicked in your inventory.
		buttons[Buttons::BT_MEGA] = std::make_unique<MapleButton>(
			chat["common"]["BtChat"], Point<int16_t>(323, -8));
		buttons[Buttons::BT_MEGA]->set_active(chatopen ? true : false);

		// The microphone, only offered when there is actually a recogniser
		// behind it - a model deployed and permission granted. A button that
		// cannot work is worse than no button, because the player spends their
		// time wondering what they did wrong.
		buttons[Buttons::BT_MIC] = std::make_unique<MapleButton>(
			chat["common"]["BtChat"], Point<int16_t>(302, -8));
		buttons[Buttons::BT_MIC]->set_active(chatopen && Speech::get().available());

		chatenter = input["layer:chatEnter"];
		chatcover = input["layer:backgrnd"];

		chatfield = Textfield(Text::A11M, Text::LEFT, Color::Name::WHITE, Rectangle<int16_t>(Point<int16_t>(62, -9), Point<int16_t>(330, 8)), 0);
		chatfield.set_state(chatopen ? Textfield::State::NORMAL : Textfield::State::DISABLED);

		chatfield.set_enter_callback(
			[&](std::string msg)
			{
				if (msg.size() > 0)
				{
					size_t last = msg.find_last_not_of(' ');

					if (last != std::string::npos)
					{
						msg.erase(last + 1);

						if (!handle_command(msg))
							GeneralChatPacket(msg, true).dispatch();

						lastentered.push_back(msg);
						lastpos = lastentered.size();
					}
					else
					{
						toggle_chatfield();
					}

					chatfield.change_text("");
				}
				else
				{
					toggle_chatfield();
				}
			}
		);

		chatfield.set_key_callback(
			KeyAction::Id::UP,
			[&]()
			{
				if (lastpos > 0)
				{
					lastpos--;
					chatfield.change_text(lastentered[lastpos]);
				}
			}
		);

		chatfield.set_key_callback(
			KeyAction::Id::DOWN,
			[&]()
			{
				if (lastentered.size() > 0 && lastpos < lastentered.size() - 1)
				{
					lastpos++;
					chatfield.change_text(lastentered[lastpos]);
				}
			}
		);

		chatfield.set_key_callback(
			KeyAction::Id::ESCAPE,
			[&]()
			{
				toggle_chatfield(false);
			}
		);

		//int16_t slider_x = 394;
		//int16_t slider_y = -80;
		//int16_t slider_height = slider_y + 56;
		//int16_t slider_unitrows = chatrows;
		//int16_t slider_rowmax = 1;
		//slider = Slider(Slider::Type::CHATBAR, Range<int16_t>(slider_y, slider_height), slider_x, slider_unitrows, slider_rowmax, [&](bool upwards) {});

		send_chatline("[Welcome] Welcome to MapleStory!!", LineType::YELLOW);

		dimension = Point<int16_t>(410, DIMENSION_Y);

		/*if (chatopen)
			dimension.shift_y(getchatbarheight());*/
	}

	void UIChatbar::draw(float inter) const
	{
		UIElement::draw_sprites(inter);

		if (chatopen)
		{
			int16_t chattop = getchattop(chatopen);

			auto pos_adj = chatfieldopen ? Point<int16_t>(0, 0) : Point<int16_t>(0, 28);

			chatspace[0].draw(position + Point<int16_t>(0, chattop) + pos_adj);

			if (chatrows > 1)
				chatspace[1].draw(DrawArgument(position + Point<int16_t>(0, -28) + pos_adj, Point<int16_t>(0, 28 + chattop)));

			chatspace[2].draw(position + Point<int16_t>(0, -28) + pos_adj);
			chatspace[3].draw(position + Point<int16_t>(0, -15 + chattop) + pos_adj);

			//slider.draw(position);

			int16_t yshift = chattop;

			for (size_t i = 0; i < chatrows; i++)
			{
				int16_t rowid = rowpos - i;

				if (!rowtexts.count(rowid))
					break;

				int16_t textheight = rowtexts.at(rowid).height() / CHATROWHEIGHT;

				while (textheight > 0)
				{
					yshift += CHATROWHEIGHT;
					textheight--;
				}

				rowtexts.at(rowid).draw(position + Point<int16_t>(9, getchattop(chatopen) - yshift - 21) + pos_adj);
			}
		}
		else
		{
			auto pos_adj = chatfieldopen ? Point<int16_t>(0, -28) : Point<int16_t>(0, 0);

			chatspace[0].draw(position + Point<int16_t>(0, -1) + pos_adj);
			chatspace[1].draw(position + Point<int16_t>(0, -1) + pos_adj);
			chatspace[2].draw(position + pos_adj);
			chatspace[3].draw(position + Point<int16_t>(0, -16) + pos_adj);

			if (rowtexts.count(rowmax))
				rowtexts.at(rowmax).draw(position + Point<int16_t>(9, -6) + pos_adj);
		}

		if (chatfieldopen)
		{
			chatcover.draw(DrawArgument(position + Point<int16_t>(0, -13), Point<int16_t>(409, 0)));
			chatenter.draw(DrawArgument(position + Point<int16_t>(0, -13), Point<int16_t>(285, 0)));
			chatfield.draw(position + Point<int16_t>(-4, -4));
		}

		UIElement::draw_buttons(inter);

		if (chatopen)
		{
			auto pos_adj = chatopen && !chatfieldopen ? Point<int16_t>(0, 28) : Point<int16_t>(0, 0);

			for (size_t i = 0; i < ChatTab::NUM_CHATTAB; i++)
				chattab_text[ChatTab::CHT_ALL + i].draw(position + Point<int16_t>(chattab_x + (i * chattab_span) + 25, chattab_y - 3) + pos_adj);
		}
	}

	void UIChatbar::update()
	{
		UIElement::update();

		// TALKING, WITH A BUBBLE OVER YOUR HEAD.
		//
		// Not "dictate into the chat box and press enter" - that is two more
		// steps on a machine with no keyboard. The player presses once, an
		// empty balloon appears above their character, it fills in as they
		// speak, and a pause sends it. The same balloon everyone else's chat
		// uses, so it looks like talking rather than like a text field.
		//
		// The pause is OURS, not the recogniser's. Vosk ends an utterance on
		// its own schedule, which is neither predictable nor tunable; watching
		// the partial text stop CHANGING is, and it is what a person actually
		// means by "finished speaking".
		if (listening)
		{
			// A final result wins outright - the recogniser has decided.
			std::string heard = Speech::get().take_phrase();
			std::string live = heard.empty() ? Speech::get().peek_partial() : heard;

			if (live != dictated)
			{
				dictated = live;
				dictation_quiet = 0;
			}
			else
			{
				dictation_quiet += Constants::TIMESTEP;
			}

			// Redrawn every tick because the balloon expires on its own after
			// four seconds, and somebody thinking mid-sentence should not have
			// it vanish on them.
			Stage::get().get_player().speak(dictated.empty() ? " " : dictated);

			bool done = !heard.empty()
				|| (!dictated.empty() && dictation_quiet >= DICTATION_PAUSE);

			// The recogniser stopping on its own with nothing to show for it -
			// no microphone, or nothing said at all.
			bool gave_up = heard.empty() && !Speech::get().is_listening();

			if (done || gave_up)
			{
				if (done && !dictated.empty())
				{
					// A small model returns bare lower case with no
					// punctuation - "where are you" - which reads as a
					// transcript rather than as somebody talking. Capitalising
					// the first letter costs nothing and is most of the
					// difference.
					std::string said = dictated;

					if (said[0] >= 'a' && said[0] <= 'z')
						said[0] = static_cast<char>(said[0] - 'a' + 'A');

					GeneralChatPacket(said, true).dispatch();
				}

				Speech::get().stop();
				Speech::get().clear_partial();

				Music::duck(false);

				listening = false;
				dictated.clear();
				dictation_quiet = 0;
			}
		}

		auto pos_adj = chatopen && !chatfieldopen ? Point<int16_t>(0, 28) : Point<int16_t>(0, 0);

		for (size_t i = 0; i < ChatTab::NUM_CHATTAB; i++)
			buttons[BT_TAB_0 + i]->set_position(Point<int16_t>(chattab_x + (i * chattab_span), chattab_y) + pos_adj);

		buttons[Buttons::BT_TAB_0 + ChatTab::NUM_CHATTAB]->set_position(Point<int16_t>(chattab_x + (ChatTab::NUM_CHATTAB * chattab_span), chattab_y) + pos_adj);
		buttons[Buttons::BT_CLOSECHAT]->set_position(closechat + Point<int16_t>(0, chattab_y) + pos_adj);

		chatfield.update(position);

		for (auto iter : message_cooldowns)
			iter.second -= Constants::TIMESTEP;
	}

	void UIChatbar::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (keycode == KeyAction::Id::RETURN)
				toggle_chatfield();
			else if (escape)
				toggle_chatfield(false);
		}
	}

	bool UIChatbar::is_in_range(Point<int16_t> cursorpos) const
	{
		auto bounds = getbounds(dimension);
		return bounds.contains(cursorpos);
	}

	Cursor::State UIChatbar::send_cursor(bool clicking, Point<int16_t> cursorpos)
	{
		if (chatopen)
		{
			if (Cursor::State new_state = chatfield.send_cursor(cursorpos, clicking))
				return new_state;

			return check_dragtop(clicking, cursorpos);
		}
		else
		{
			return UIDragElement::send_cursor(clicking, cursorpos);
		}
	}

	UIElement::Type UIChatbar::get_type() const
	{
		return TYPE;
	}

	Cursor::State UIChatbar::check_dragtop(bool clicking, Point<int16_t> cursorpos)
	{
		Rectangle<int16_t> bounds = getbounds(dimension);
		Point<int16_t> bounds_lt = bounds.get_left_top();
		Point<int16_t> bounds_rb = bounds.get_right_bottom();

		int16_t chattab_height = 20;
		int16_t bounds_rb_y = bounds_rb.y();
		int16_t bounds_lt_y = bounds_lt.y() + chattab_height;

		auto chattop_rb = Point<int16_t>(bounds_rb.x() - 1, bounds_rb_y - 27);
		auto chattop = Rectangle<int16_t>(Point<int16_t>(bounds_lt.x() + 1, bounds_lt_y), chattop_rb);

		auto chattopleft = Rectangle<int16_t>(Point<int16_t>(bounds_lt.x(), bounds_lt_y), Point<int16_t>(bounds_lt.x(), chattop_rb.y()));
		auto chattopright = Rectangle<int16_t>(Point<int16_t>(chattop_rb.x() + 1, bounds_lt_y), Point<int16_t>(chattop_rb.x() + 1, chattop_rb.y()));
		auto chatleft = Rectangle<int16_t>(Point<int16_t>(bounds_lt.x(), bounds_lt_y), Point<int16_t>(bounds_lt.x(), bounds_lt_y + bounds_rb_y));
		auto chatright = Rectangle<int16_t>(Point<int16_t>(chattop_rb.x() + 1, bounds_lt_y), Point<int16_t>(chattop_rb.x() + 1, bounds_lt_y + bounds_rb_y));

		bool in_chattop = chattop.contains(cursorpos);
		bool in_chattopleft = chattopleft.contains(cursorpos);
		bool in_chattopright = chattopright.contains(cursorpos);
		bool in_chatleft = chatleft.contains(cursorpos);
		bool in_chatright = chatright.contains(cursorpos);

		if (dragchattop)
		{
			if (clicking)
			{
				int16_t ydelta = cursorpos.y() - bounds_rb_y + 10;

				while (ydelta > 0 && chatrows > MINCHATROWS)
				{
					chatrows--;
					ydelta -= CHATROWHEIGHT;
				}

				while (ydelta < 0 && chatrows < MAXCHATROWS)
				{
					chatrows++;
					ydelta += CHATROWHEIGHT;
				}

				//slider.setrows(rowpos, chatrows, rowmax);
				//slider.setvertical(Range<int16_t>(0, CHATROWHEIGHT * chatrows - 14));

				chattab_y = getchattop(chatopen) - 33;
				//dimension.set_y(getchatbarheight());

				return Cursor::State::CLICKING;
			}
			else
			{
				dragchattop = false;
			}
		}
		else if (in_chattop)
		{
			if (clicking)
			{
				dragchattop = true;

				return Cursor::State::CLICKING;
			}
			else
			{
				return Cursor::State::CHATBARVDRAG;
			}
		}
		else if (in_chattopleft)
		{
			if (clicking)
			{
				//dragchattopleft = true;

				return Cursor::State::CLICKING;
			}
			else
			{
				return Cursor::State::CHATBARBRTLDRAG;
			}
		}
		else if (in_chattopright)
		{
			if (clicking)
			{
				//dragchattopright = true;

				return Cursor::State::CLICKING;
			}
			else
			{
				return Cursor::State::CHATBARBLTRDRAG;
			}
		}
		else if (in_chatleft)
		{
			if (clicking)
			{
				//dragchatleft = true;

				return Cursor::State::CLICKING;
			}
			else
			{
				return Cursor::State::CHATBARHDRAG;
			}
		}
		else if (in_chatright)
		{
			if (clicking)
			{
				//dragchatright = true;

				return Cursor::State::CLICKING;
			}
			else
			{
				return Cursor::State::CHATBARHDRAG;
			}
		}

		return UIDragElement::send_cursor(clicking, cursorpos);
	}

	bool UIChatbar::indragrange(Point<int16_t> cursorpos) const
	{
		auto bounds = getbounds(dragarea);

		return bounds.contains(cursorpos);
	}

	void UIChatbar::set_last_whisperer(const std::string& name)
	{
		last_whisperer = name;
	}

	void UIChatbar::send_chatline(const std::string& line, LineType type)
	{
		rowmax++;
		rowpos = rowmax;

		//slider.setrows(rowpos, chatrows, rowmax);

		Color::Name color;

		switch (type)
		{
		case LineType::RED:
			color = Color::Name::DARKRED;
			break;
		case LineType::BLUE:
			color = Color::Name::MEDIUMBLUE;
			break;
		case LineType::YELLOW:
			color = Color::Name::YELLOW;
			break;
		default:
			color = Color::Name::WHITE;
			break;
		}

		rowtexts.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(rowmax),
			std::forward_as_tuple(Text::Font::A11M, Text::Alignment::LEFT, color, line, 480)
		);
	}

	void UIChatbar::display_message(Messages::Type line, UIChatbar::LineType type)
	{
		if (message_cooldowns[line] > 0)
			return;

		std::string message = Messages::messages[line];
		send_chatline(message, type);

		message_cooldowns[line] = MESSAGE_COOLDOWN;
	}

	void UIChatbar::toggle_chat()
	{
		chatopen_persist = !chatopen_persist;
		toggle_chat(chatopen_persist);
	}

	void UIChatbar::toggle_chat(bool chat_open)
	{
		if (!chat_open && chatopen_persist)
			return;

		chatopen = chat_open;

		if (!chatopen && chatfieldopen)
			toggle_chatfield();

		buttons[Buttons::BT_OPENCHAT]->set_active(!chat_open);
		buttons[Buttons::BT_CLOSECHAT]->set_active(chat_open);

		for (size_t i = 0; i < ChatTab::NUM_CHATTAB; i++)
			buttons[Buttons::BT_TAB_0 + i]->set_active(chat_open);

		buttons[Buttons::BT_TAB_0 + ChatTab::NUM_CHATTAB]->set_active(chat_open);
	}

	void UIChatbar::toggle_chatfield()
	{
		chatfieldopen = !chatfieldopen;
		toggle_chatfield(chatfieldopen);
	}

	void UIChatbar::toggle_chatfield(bool chatfield_open)
	{
		chatfieldopen = chatfield_open;

		toggle_chat(chatfieldopen);

		if (chatfieldopen)
		{
			buttons[Buttons::BT_CHAT]->set_active(true);
			buttons[Buttons::BT_HELP]->set_active(true);
			buttons[Buttons::BT_MEGA]->set_active(true);
			buttons[Buttons::BT_MIC]->set_active(true && Speech::get().available());
			buttons[Buttons::BT_LINK]->set_active(true);
			buttons[Buttons::BT_CHAT_TARGET]->set_active(true);

			chatfield.set_state(Textfield::State::FOCUSED);

			//dimension.shift_y(getchatbarheight());
		}
		else
		{
			buttons[Buttons::BT_CHAT]->set_active(false);
			buttons[Buttons::BT_HELP]->set_active(false);
			buttons[Buttons::BT_MEGA]->set_active(false);
			buttons[Buttons::BT_MIC]->set_active(false && Speech::get().available());
			buttons[Buttons::BT_LINK]->set_active(false);
			buttons[Buttons::BT_CHAT_TARGET]->set_active(false);

			chatfield.set_state(Textfield::State::DISABLED);
			chatfield.change_text("");

			//dimension.set_y(DIMENSION_Y);
		}
	}

	void UIChatbar::start_dictation()
	{
		if (listening)
		{
			Speech::get().stop();
			Music::duck(false);
			listening = false;
			return;
		}

		listening = Speech::get().start();

		// The speaker is an inch from the microphone on these machines, so the
		// recogniser hears the soundtrack too and does noticeably worse for it.
		if (listening)
			Music::duck(true);
	}

	bool UIChatbar::is_chatopen()
	{
		return chatopen;
	}

	bool UIChatbar::is_chatfieldopen()
	{
		return chatfieldopen;
	}

	Button::State UIChatbar::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::BT_MIC:
			// Straight into the chat box, which is already the thing that
			// talks to everyone standing on this map.
			if (listening)
			{
				Speech::get().stop();
				listening = false;
			}
			else
			{
				listening = Speech::get().start();
			}
			return Button::State::NORMAL;
		case Buttons::BT_MEGA:
			// Toggled rather than always opened, so the same button puts it
			// away again - there is no room on a handheld for a window you
			// can only close from inside itself.
			UI::get().emplace<UIMegaphone>();
			return Button::State::NORMAL;
		case Buttons::BT_OPENCHAT:
		case Buttons::BT_CLOSECHAT:
			toggle_chat();
			break;
		case Buttons::BT_TAB_0:
		case Buttons::BT_TAB_1:
		case Buttons::BT_TAB_2:
		case Buttons::BT_TAB_3:
		case Buttons::BT_TAB_4:
		case Buttons::BT_TAB_5:
			for (size_t i = 0; i < ChatTab::NUM_CHATTAB; i++)
			{
				buttons[Buttons::BT_TAB_0 + i]->set_state(Button::State::NORMAL);
				chattab_text[ChatTab::CHT_ALL + i].change_color(Color::Name::DUSTYGRAY);
			}

			chattab_text[buttonid - Buttons::BT_TAB_0].change_color(Color::Name::WHITE);

			return Button::State::PRESSED;
		}

		Setting<Chatopen>::get().save(chatopen);

		return Button::State::NORMAL;
	}

	int16_t UIChatbar::getchattop(bool chat_open) const
	{
		if (chat_open)
			return getchatbarheight() * -1;
		else
			return -1;
	}

	int16_t UIChatbar::getchatbarheight() const
	{
		return 15 + chatrows * CHATROWHEIGHT;
	}

	Rectangle<int16_t> UIChatbar::getbounds(Point<int16_t> additional_area) const
	{
		int16_t screen_adj = (chatopen) ? 35 : 16;

		auto absp = position + Point<int16_t>(0, getchattop(chatopen));
		auto da = absp + additional_area;

		absp = Point<int16_t>(absp.x(), absp.y() - screen_adj);
		da = Point<int16_t>(da.x(), da.y());

		return Rectangle<int16_t>(absp, da);
	}
}