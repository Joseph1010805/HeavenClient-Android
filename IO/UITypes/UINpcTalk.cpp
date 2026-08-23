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
#include "UINpcTalk.h"

#include "../UI.h"

#include "../Components/MapleButton.h"
#include "../Gameplay/Stage.h"

#include "../Net/Packets/NpcInteractionPackets.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UINpcTalk::UINpcTalk() : offset(0), unitrows(0), rowmax(0), show_slider(false), draw_text(false), formatted_text(""), formatted_text_pos(0), timestep(0), hovered_selection(-1)
	{
		nl::node UIWindow2 = nl::nx::ui["UIWindow2.img"];
		nl::node UtilDlgEx = UIWindow2["UtilDlgEx"];

		top = UtilDlgEx["t"];
		fill = UtilDlgEx["c"];
		bottom = UtilDlgEx["s"];
		nametag = UtilDlgEx["bar"];

		min_height = 8 * fill.height() + 14;

		buttons[Buttons::ALLLEVEL] = std::make_unique<MapleButton>(UtilDlgEx["BtAllLevel"]);
		buttons[Buttons::CLOSE] = std::make_unique<MapleButton>(UtilDlgEx["BtClose"]);
		buttons[Buttons::MYLEVEL] = std::make_unique<MapleButton>(UtilDlgEx["BtMyLevel"]);
		buttons[Buttons::NEXT] = std::make_unique<MapleButton>(UtilDlgEx["BtNext"]);

		// TODO: Replace when _inlink is fixed
		//buttons[Buttons::NO] = std::make_unique<MapleButton>(UtilDlgEx["BtNo"]);

		nl::node Quest = UIWindow2["Quest"];

		buttons[Buttons::NO] = std::make_unique<MapleButton>(Quest["BtNo"]);
		buttons[Buttons::OK] = std::make_unique<MapleButton>(UtilDlgEx["BtOK"]);
		buttons[Buttons::PREV] = std::make_unique<MapleButton>(UtilDlgEx["BtPrev"]);
		buttons[Buttons::QAFTER] = std::make_unique<MapleButton>(UtilDlgEx["BtQAfter"]);
		buttons[Buttons::QCNO] = std::make_unique<MapleButton>(UtilDlgEx["BtQCNo"]);
		buttons[Buttons::QCYES] = std::make_unique<MapleButton>(UtilDlgEx["BtQCYes"]);
		buttons[Buttons::QGIVEUP] = std::make_unique<MapleButton>(UtilDlgEx["BtQGiveup"]);
		buttons[Buttons::QNO] = std::make_unique<MapleButton>(UtilDlgEx["BtQNo"]);
		buttons[Buttons::QSTART] = std::make_unique<MapleButton>(UtilDlgEx["BtQStart"]);
		buttons[Buttons::QYES] = std::make_unique<MapleButton>(UtilDlgEx["BtQYes"]);
		buttons[Buttons::YES] = std::make_unique<MapleButton>(UtilDlgEx["BtYes"]);

		name = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);

		onmoved = [&](bool upwards)
		{
			int16_t shift = upwards ? -unitrows : unitrows;
			bool above = offset + shift >= 0;
			bool below = offset + shift <= rowmax - unitrows;

			if (above && below)
				offset += shift;
		};

		UI::get().remove_textfield();
	}

	void UINpcTalk::draw(float inter) const
	{
		Point<int16_t> drawpos = position;
		top.draw(drawpos);
		drawpos.shift_y(top.height());
		fill.draw(DrawArgument(drawpos, Point<int16_t>(0, height)));
		drawpos.shift_y(height);
		bottom.draw(drawpos);
		drawpos.shift_y(bottom.height());

		UIElement::draw(inter);

		int16_t speaker_y = (top.height() + height + bottom.height()) / 2;
		Point<int16_t> speaker_pos = position + Point<int16_t>(22, 11 + speaker_y);
		Point<int16_t> center_pos = speaker_pos + Point<int16_t>(nametag.width() / 2, 0);

		speaker.draw(DrawArgument(center_pos, true));
		nametag.draw(speaker_pos);
		name.draw(center_pos + Point<int16_t>(0, -4));

		if (show_slider)
		{
			int16_t text_min_height = position.y() + top.height() - 1;
			text.draw(position + Point<int16_t>(162, 19 - offset * 400), Range<int16_t>(text_min_height, text_min_height + height - 18));
			slider.draw(position);
		}
		else
		{
			int16_t y_adj = height - min_height;
			Point<int16_t> body = position + Point<int16_t>(166, 48 - y_adj);

			text.draw(body);
			draw_selections(body + Point<int16_t>(0, text.height()));
		}
	}

	// Lays the choices out one per row beneath the message, remembering where
	// each landed so send_cursor can test the pointer against it.
	int16_t UINpcTalk::draw_selections(Point<int16_t> at) const
	{
		if (selections.empty())
			return at.y();

		constexpr int16_t ROW_GAP = 2;
		constexpr int16_t ROW_PAD = 1;

		int16_t y = at.y() + ROW_GAP;

		for (size_t i = 0; i < selections.size(); i++)
		{
			const Selection& sel = selections[i];
			int16_t row_h = sel.label.height();

			sel.bounds = Rectangle<int16_t>(
				Point<int16_t>(at.x(), y),
				Point<int16_t>(at.x() + sel.label.width(), y + row_h)
			);

			if (static_cast<int32_t>(i) == hovered_selection)
			{
				ColorBox highlight(
					static_cast<int16_t>(sel.label.width() + ROW_PAD * 2),
					row_h, Color::Name::LIGHTGREY, 0.45f);

				highlight.draw(DrawArgument(Point<int16_t>(at.x() - ROW_PAD, y)));
			}

			sel.label.draw(Point<int16_t>(at.x(), y));

			y = static_cast<int16_t>(y + row_h + ROW_GAP);
		}

		return y;
	}

	void UINpcTalk::update()
	{
		UIElement::update();

		if (draw_text)
		{
			if (timestep > 4)
			{
				if (formatted_text_pos < formatted_text.size())
				{
					std::string t = text.get_text();
					char c = formatted_text[formatted_text_pos];

					text.change_text(t + c);

					formatted_text_pos++;
					timestep = 0;
				}
				else
				{
					draw_text = false;
				}
			}
			else
			{
				timestep++;
			}
		}
	}

	Button::State UINpcTalk::button_pressed(uint16_t buttonid)
	{
		deactivate();

		switch (type)
		{
		case TalkType::SENDNEXT:
		case TalkType::SENDOK:
			// Type = 0
			switch (buttonid)
			{
			case Buttons::CLOSE:
				NpcTalkMorePacket(type, -1).dispatch();
				break;
			case Buttons::NEXT:
			case Buttons::OK:
				NpcTalkMorePacket(type, 1).dispatch();
				break;
			}
			break;
		case TalkType::SENDNEXTPREV:
			// Type = 0
			switch (buttonid)
			{
			case Buttons::CLOSE:
				NpcTalkMorePacket(type, -1).dispatch();
				break;
			case Buttons::NEXT:
				NpcTalkMorePacket(type, 1).dispatch();
				break;
			case Buttons::PREV:
				NpcTalkMorePacket(type, 0).dispatch();
				break;
			}
			break;
		case TalkType::SENDYESNO:
			// Type = 1
			switch (buttonid)
			{
			case Buttons::CLOSE:
				NpcTalkMorePacket(type, -1).dispatch();
				break;
			case Buttons::NO:
				NpcTalkMorePacket(type, 0).dispatch();
				break;
			case Buttons::YES:
				NpcTalkMorePacket(type, 1).dispatch();
				break;
			}
			break;
		case TalkType::SENDACCEPTDECLINE:
			// Type = 1
			switch (buttonid)
			{
			case Buttons::CLOSE:
				NpcTalkMorePacket(type, -1).dispatch();
				break;
			case Buttons::QNO:
				NpcTalkMorePacket(type, 0).dispatch();
				break;
			case Buttons::QYES:
				NpcTalkMorePacket(type, 1).dispatch();
				break;
			}
			break;
		case TalkType::SENDGETTEXT:
			// TODO: What is this?
			break;
		case TalkType::SENDGETNUMBER:
			// Type = 3
			switch (buttonid)
			{
			case Buttons::CLOSE:
				NpcTalkMorePacket(type, 0).dispatch();
				break;
			case Buttons::OK:
				NpcTalkMorePacket(type, 1).dispatch();
				break;
			}
			break;
		case TalkType::SENDSIMPLE:
			// Type = 4
			switch (buttonid)
			{
			case Buttons::CLOSE:
				NpcTalkMorePacket(type, 0).dispatch();
				break;
			default:
				NpcTalkMorePacket(0).dispatch(); // TODO: Selection
				break;
			}
			break;
		default:
			break;
		}

		return Button::State::NORMAL;
	}

	Cursor::State UINpcTalk::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Point<int16_t> cursor_relative = cursorpos - position;

		if (show_slider && slider.isenabled())
			if (Cursor::State sstate = slider.send_cursor(cursor_relative, clicked))
				return sstate;

		// A choice takes precedence over the rest of the window: the rows sit
		// over the message area, so testing them first is what makes them
		// clickable at all.
		if (!selections.empty() && !draw_text)
		{
			hovered_selection = -1;

			for (size_t i = 0; i < selections.size(); i++)
			{
				if (!selections[i].bounds.contains(cursorpos))
					continue;

				hovered_selection = static_cast<int32_t>(i);

				if (clicked)
				{
					int32_t chosen = selections[i].index;

					deactivate();
					NpcTalkMorePacket(chosen).dispatch();

					return Cursor::State::CLICKING;
				}

				return Cursor::State::CANCLICK;
			}
		}

		Cursor::State estate = UIElement::send_cursor(clicked, cursorpos);

		if (estate == Cursor::State::CLICKING && clicked && draw_text)
		{
			// Skip the typewriter and show the whole message at once. The
			// choices only become clickable once it has finished, so that a
			// tap meant to hurry the text along cannot pick one by accident.
			draw_text = false;
			text.change_text(formatted_text);
		}

		return estate;
	}

	void UINpcTalk::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			deactivate();

			NpcTalkMorePacket(type, 0).dispatch();
		}
	}

	UIElement::Type UINpcTalk::get_type() const
	{
		return TYPE;
	}

	UINpcTalk::TalkType UINpcTalk::get_by_value(int8_t value)
	{
		if (value > TalkType::NONE && value < TalkType::LENGTH)
			return static_cast<TalkType>(value);

		return TalkType::NONE;
	}

	// Turns a raw NPC message into what should actually be shown, and pulls
	// out any choices it offers along the way.
	//
	// The server writes these messages in a small markup the client is
	// expected to understand:
	//
	//   #p<id>#   the NPC's name          #h #    the player's name
	//   #t<id>#   an item's name          #m<id># a map's name
	//   #L<n>#..#l  a selectable choice
	//   #b #k #r #g #d #e #n #f #v #z #c  colour and style switches
	//
	// None of it was handled except the first three, and those by searching
	// for a code and then for the NEXT '#' anywhere in the string - which in
	// a message carrying several codes deletes whatever happens to lie
	// between them. That is why choices arrived welded to the end of the
	// sentence with their markers half-eaten: `#L1#Please` showed up as
	// `?1lease`.
	//
	// This walks the string once instead, which is the only way to get it
	// right when the codes can appear in any order.
	std::string UINpcTalk::format_text(const std::string& tx, const int32_t& npcid)
	{
		std::string out;
		selections.clear();

		// Set while inside `#L<n>#...#l`, so the wording goes to the choice
		// rather than into the body of the message.
		bool in_selection = false;
		int32_t selection_index = 0;
		std::string selection_text;

		auto emit = [&](const std::string& piece)
		{
			if (in_selection)
				selection_text += piece;
			else
				out += piece;
		};

		for (size_t i = 0; i < tx.size(); )
		{
			if (tx[i] != '#' || i + 1 >= tx.size())
			{
				// A literal carriage return would draw as a stray glyph.
				if (tx[i] != '\r')
					emit(std::string(1, tx[i]));

				i++;
				continue;
			}

			char code = tx[i + 1];

			// Codes that read a number up to a closing '#'.
			if (code == 'p' || code == 't' || code == 'm' || code == 'o' || code == 'i')
			{
				size_t close = tx.find('#', i + 2);

				if (close == std::string::npos)
				{
					emit(std::string(1, tx[i]));
					i++;
					continue;
				}

				std::string digits = tx.substr(i + 2, close - i - 2);
				int32_t id = 0;

				try
				{
					id = std::stoi(digits);
				}
				catch (...)
				{
					i = close + 1;
					continue;
				}

				switch (code)
				{
				case 'p':
					emit(nl::nx::string["Npc.img"][std::to_string(id)]["name"]);
					break;
				case 'm':
					emit(nl::nx::string["Map.img"][std::to_string(id)]["mapName"]);
					break;
				default:
					// Items live in one of several files depending on kind;
					// the consumables one covers what quests hand out.
					emit(nl::nx::string["Consume.img"][std::to_string(id)]["name"]);
					break;
				}

				i = close + 1;
				continue;
			}

			// A choice opens with #L<n># and closes with #l.
			if (code == 'L')
			{
				size_t close = tx.find('#', i + 2);

				if (close != std::string::npos)
				{
					try
					{
						selection_index = std::stoi(tx.substr(i + 2, close - i - 2));
					}
					catch (...)
					{
						selection_index = static_cast<int32_t>(selections.size());
					}

					in_selection = true;
					selection_text.clear();
					i = close + 1;
					continue;
				}
			}

			if (code == 'l')
			{
				if (in_selection)
				{
					Selection sel;
					sel.index = selection_index;
					sel.label = Text(Text::Font::A12M, Text::Alignment::LEFT,
						Color::Name::BLUE, selection_text);
					selections.push_back(std::move(sel));

					in_selection = false;
					selection_text.clear();
				}

				i += 2;
				continue;
			}

			if (code == 'h')
			{
				emit(Stage::get().get_player().get_name());

				// Written as `#h #`, so step over the trailing marker.
				size_t close = tx.find('#', i + 2);
				i = (close == std::string::npos) ? i + 2 : close + 1;
				continue;
			}

			// Colour and style switches, which carry no text of their own.
			if (std::string("bkrgdenfvzc").find(code) != std::string::npos)
			{
				i += 2;
				continue;
			}

			// Anything unrecognised: drop the marker, keep the letter, so an
			// unknown code costs a '#' rather than a word.
			emit(std::string(1, code));
			i += 2;
		}

		// An unterminated choice still counts - better a clickable line than
		// wording that vanishes.
		if (in_selection && !selection_text.empty())
		{
			Selection sel;
			sel.index = selection_index;
			sel.label = Text(Text::Font::A12M, Text::Alignment::LEFT,
				Color::Name::BLUE, selection_text);
			selections.push_back(std::move(sel));
		}

		return out;
	}

	void UINpcTalk::change_text(int32_t npcid, int8_t msgtype, int16_t, int8_t speakerbyte, const std::string& tx)
	{
		type = get_by_value(msgtype);

		timestep = 0;
		draw_text = true;
		formatted_text_pos = 0;
		formatted_text = format_text(tx, npcid);

		text = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::DARKGREY, formatted_text, 320);

		int16_t text_height = text.height();

		text.change_text("");

		if (speakerbyte == 0)
		{
			std::string strid = std::to_string(npcid);
			strid.insert(0, 7 - strid.size(), '0');
			strid.append(".img");

			speaker = nl::nx::npc[strid]["stand"]["0"];

			std::string namestr = nl::nx::string["Npc.img"][std::to_string(npcid)]["name"];
			name.change_text(namestr);
		}
		else
		{
			speaker = Texture();
			name.change_text("");
		}

		height = min_height;
		show_slider = false;

		if (text_height > height)
		{
			if (text_height > MAX_HEIGHT)
			{
				height = MAX_HEIGHT;
				show_slider = true;
				rowmax = text_height / 400 + 1;
				unitrows = 1;

				int16_t slider_y = top.height() - 7;
				slider = Slider(Slider::Type::DEFAULT, Range<int16_t>(slider_y, slider_y + height - 20), top.width() - 26, unitrows, rowmax, onmoved);
			}
			else
			{
				height = text_height;
			}
		}

		for (auto& button : buttons)
		{
			button.second->set_active(false);
			button.second->set_state(Button::State::NORMAL);
		}

		int16_t y_cord = height + 48;

		buttons[Buttons::CLOSE]->set_position(Point<int16_t>(9, y_cord));
		buttons[Buttons::CLOSE]->set_active(true);

		switch (type)
		{
		case TalkType::SENDOK:
			buttons[Buttons::OK]->set_position(Point<int16_t>(471, y_cord));
			buttons[Buttons::OK]->set_active(true);
			break;
		case TalkType::SENDYESNO:
		{
			Point<int16_t> yes_position = Point<int16_t>(389, y_cord);

			buttons[Buttons::YES]->set_position(yes_position);
			buttons[Buttons::YES]->set_active(true);

			buttons[Buttons::NO]->set_position(yes_position + Point<int16_t>(65, 0));
			buttons[Buttons::NO]->set_active(true);
			break;
		}
		case TalkType::SENDNEXT:
		case TalkType::SENDNEXTPREV:
		case TalkType::SENDACCEPTDECLINE:
		case TalkType::SENDGETTEXT:
		case TalkType::SENDGETNUMBER:
		case TalkType::SENDSIMPLE:
		default:
			break;
		}

		position = Point<int16_t>(400 - top.width() / 2, 240 - height / 2);
		dimension = Point<int16_t>(top.width(), height + 120);
	}
}