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

#include "../../Data/ItemData.h"

#include "../Components/MapleButton.h"
#include "../Gameplay/Stage.h"

#include "../Net/Packets/NpcInteractionPackets.h"

#include <nlnx/nx.hpp>

namespace ms
{
	// Where the first line sits, measured down from the frame's origin. Used
	// by draw() to place the body and by the sizing to work out how big a box
	// that body needs - they must agree, so there is one number.
	constexpr int16_t BODY_TOP = 19;

	// A CHOICE IS A BUTTON, NOT A LINE OF TEXT.
	//
	// These used to be bare labels, and the hitbox was the text itself - as
	// tall as the glyphs and only as wide as the words happened to be. On a
	// handheld held at arm's length, with a stylus, "Magician - Ellinia" is a
	// 16-pixel-tall target and "No" is barely there at all.
	//
	// So each one is drawn as a filled bar across the body: a fixed height a
	// thumb can find, the full width whatever the wording, and the SAME gap
	// between every pair so the eye can count them.
	constexpr int16_t ROW_H = 51;
	constexpr int16_t ROW_GAP = 6;

	// How far the bar runs. The body starts 166 in and the frame's inner edge
	// is a little under 500, so this fills it without touching the border.
	constexpr int16_t ROW_W = 316;

	// How much larger the frame's own buttons are drawn than their artwork.
	constexpr float BUTTON_SCALE = 1.5f;

	UINpcTalk::UINpcTalk() : offset(0), scroll(0), content_height(0), unitrows(0), rowmax(0), show_slider(false), draw_text(false), formatted_text(""), formatted_text_pos(0), timestep(0), hovered_selection(-1)
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

		// BIG ENOUGH FOR A THUMB.
		//
		// NEXT, OK, YES, NO and the rest are drawn from artwork made for a
		// mouse pointer on a desktop monitor - about 60x20. On a handheld held
		// at arm's length, answered with a stylus or a thumb, that is a hard
		// target to hit and an easy one to miss onto whatever is beside it.
		//
		// Scaled rather than redrawn: Button::set_scale is honoured by
		// MapleButton's draw AND by its bounds, so the picture and the press
		// area grow together. Half again as big, matching the choice bars.
		for (auto& button : buttons)
			button.second->set_scale(BUTTON_SCALE);

		name = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);

		onmoved = [&](bool upwards)
		{
			int16_t shift = upwards ? -unitrows : unitrows;
			bool above = offset + shift >= 0;
			bool below = offset + shift <= rowmax - unitrows;

			if (above && below)
			{
				offset += shift;

				// The slider moves a row; the body moves pixels. Kept in step
				// here rather than recomputed at draw time, so there is one
				// place that decides where the content sits.
				scroll = static_cast<int16_t>(offset * SCROLL_STEP);
			}
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

		// The inside of the frame, in screen coordinates. Everything the body
		// draws is clipped to this - without it a long menu simply carries on
		// past the bottom edge and is drawn over the map.
		int16_t clip_top = position.y() + top.height() - 1;
		Range<int16_t> clip(clip_top, clip_top + height - 18);

		// ONE RULE FOR EVERY MESSAGE, SCROLLING OR NOT.
		//
		// There used to be two branches here and they disagreed about where
		// the body starts. The scrolling one began at 19 and clipped; the
		// other began at `48 - (height - min_height)` and did not clip at all.
		//
		// That second expression moves the text UP by however much the box
		// GREW - and the box grows to fit its content. So the more there was
		// to read, the further above the frame it was drawn: Robin's quiz put
		// its question out over the trees with only the answers left inside.
		// It was never wrong for a short line, which is why it survived.
		//
		// The frame's top edge is a fixed place. The body starts just inside
		// it, always, and `scroll` is the only thing that moves it - so text
		// cannot leave the window, and when there is more than fits, the
		// slider is how you reach the rest.
		Point<int16_t> body = position + Point<int16_t>(166, BODY_TOP - scroll);

		text.draw(body, clip);
		draw_inline_icons(body, clip);
		draw_selections(body + Point<int16_t>(0, text.height()), clip);

		if (show_slider)
		{
			slider.draw(position);
		}
	}

	// THE PICTURES THE SENTENCE ASKED FOR.
	//
	// The layout reserved a square wherever a `#v<id>#` and friends appeared
	// and recorded what belongs there; this opens the right NX file and
	// stamps the bitmap into that square. Split this way because the layout
	// knows about glyphs and nothing else, and should stay that way.
	//
	// Ported from OpenStory's draw_inline_icons.
	void UINpcTalk::draw_inline_icons(Point<int16_t> origin,
		Range<int16_t> clip) const
	{
		for (const Text::Layout::Image& img : text.images())
		{
			if (img.item_id <= 0)
				continue;

			Texture tex;

			switch (img.kind)
			{
			case Text::Layout::ImageKind::ITEM:
				// The small inventory icon, NOT iconRaw - that is the full
				// size artwork and dwarfs the slot.
				tex = ItemData::get(img.item_id).get_icon(false);
				break;
			case Text::Layout::ImageKind::QUEST:
			{
				nl::node node = nl::nx::ui["UIWindow.img"]["QuestIcon"]
					[std::to_string(img.item_id)];

				if (node)
					tex = Texture(node);

				break;
			}
			case Text::Layout::ImageKind::SKILL:
			{
				// A skill icon is filed under its job, and the job is the
				// leading digits of the skill id.
				std::string job = std::to_string(img.item_id / 10000);

				while (job.size() < 3)
					job.insert(0, 1, '0');

				nl::node node = nl::nx::skill[job + ".img"]["skill"]
					[std::to_string(img.item_id)]["icon"];

				if (node)
					tex = Texture(node);

				break;
			}
			default:
				break;
			}

			if (!tex.is_valid())
				continue;

			Point<int16_t> size = tex.get_dimensions();

			if (size.x() <= 0 || size.y() <= 0)
				continue;

			// Icons come in several sizes and the slot is one size, so the
			// bitmap is scaled to fit rather than allowed to spill over the
			// words around it.
			float scale = 1.0f;
			int16_t largest = std::max(size.x(), size.y());

			if (largest > img.size)
				scale = static_cast<float>(img.size) / static_cast<float>(largest);

			int16_t w = static_cast<int16_t>(size.x() * scale);
			int16_t h = static_cast<int16_t>(size.y() * scale);

			Point<int16_t> slot = origin + img.pos;

			Point<int16_t> corner(
				static_cast<int16_t>(slot.x() + (img.size - w) / 2),
				static_cast<int16_t>(slot.y() + (img.size - h) / 2));

			// Anything not wholly inside the frame is skipped, so a scrolled
			// line cannot paint a picture over the border.
			if (corner.y() < clip.first() || corner.y() + h > clip.second())
				continue;

			// draw() pins the texture's ORIGIN at the position given and
			// scales about it, so the scaled origin is added back to land the
			// bitmap's top-left corner exactly where it belongs.
			Point<int16_t> beginning = tex.get_origin();

			tex.draw(DrawArgument(Point<int16_t>(
				static_cast<int16_t>(corner.x() + beginning.x() * scale),
				static_cast<int16_t>(corner.y() + beginning.y() * scale)),
				scale, scale, 1.0f));
		}
	}

	// Lays the choices out one per row beneath the message, remembering where
	// each landed so send_cursor can test the pointer against it.
	int16_t UINpcTalk::selections_height() const
	{
		if (selections.empty())
			return 0;

		// Every row is the same height now, so this is arithmetic rather than
		// a walk - and it CANNOT disagree with what draw_selections lays out,
		// which is how rows used to end up unreachable below the frame.
		int16_t rows = static_cast<int16_t>(selections.size());

		return static_cast<int16_t>(ROW_GAP + rows * (ROW_H + ROW_GAP));
	}

	int16_t UINpcTalk::draw_selections(Point<int16_t> at, Range<int16_t> clip) const
	{
		if (selections.empty())
			return at.y();

		int16_t y = static_cast<int16_t>(at.y() + ROW_GAP);

		for (size_t i = 0; i < selections.size(); i++)
		{
			const Selection& sel = selections[i];

			// Off the top or the bottom of the frame: not drawn, and NOT
			// given a hitbox. An empty rectangle can contain nothing, so a row
			// scrolled out of sight cannot be clicked through the chrome.
			if (y + ROW_H < clip.first() || y > clip.second())
			{
				sel.bounds = Rectangle<int16_t>();
				y = static_cast<int16_t>(y + ROW_H + ROW_GAP);
				continue;
			}

			// THE WHOLE BAR IS THE TARGET, not the words on it.
			sel.bounds = Rectangle<int16_t>(
				Point<int16_t>(at.x(), y),
				Point<int16_t>(static_cast<int16_t>(at.x() + ROW_W),
					static_cast<int16_t>(y + ROW_H)));

			bool lit = (static_cast<int32_t>(i) == hovered_selection);

			GraphicsGL::get().drawrectangle(at.x(), y, ROW_W, ROW_H,
				lit ? 0.78f : 0.90f, lit ? 0.84f : 0.92f,
				lit ? 0.95f : 0.96f, 1.0f);

			// A border, so a row reads as something to press rather than as a
			// tinted patch of the message area.
			GraphicsGL::get().drawrectangle(at.x(), y, ROW_W, 1,
				0.55f, 0.60f, 0.70f, 1.0f);
			GraphicsGL::get().drawrectangle(at.x(),
				static_cast<int16_t>(y + ROW_H - 1), ROW_W, 1,
				0.55f, 0.60f, 0.70f, 1.0f);

			// Centred in the bar rather than sitting on its top edge.
			int16_t text_y = static_cast<int16_t>(
				y + (ROW_H - sel.label.height()) / 2);

			sel.label.draw(Point<int16_t>(static_cast<int16_t>(at.x() + 10), text_y));

			y = static_cast<int16_t>(y + ROW_H + ROW_GAP);
		}

		return y;
	}

	void UINpcTalk::update()
	{
		if (settle > 0)
			settle--;

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
				NpcTalkMorePacket(wire_type, -1).dispatch();
				break;
			case Buttons::NEXT:
			case Buttons::OK:
				NpcTalkMorePacket(wire_type, 1).dispatch();
				break;
			}
			break;
		case TalkType::SENDNEXTPREV:
		case TalkType::SENDPREV:
			// Type = 0
			switch (buttonid)
			{
			case Buttons::CLOSE:
				NpcTalkMorePacket(wire_type, -1).dispatch();
				break;
			case Buttons::NEXT:
			// OK IS A FORWARD BUTTON, and on a prev-only page it is the only
			// one - see the note where the buttons are activated.
			case Buttons::OK:
				NpcTalkMorePacket(wire_type, 1).dispatch();
				break;
			case Buttons::PREV:
				NpcTalkMorePacket(wire_type, 0).dispatch();
				break;
			}
			break;
		case TalkType::SENDYESNO:
			// Type = 1
			switch (buttonid)
			{
			case Buttons::CLOSE:
				NpcTalkMorePacket(wire_type, -1).dispatch();
				break;
			case Buttons::NO:
				NpcTalkMorePacket(wire_type, 0).dispatch();
				break;
			case Buttons::YES:
				NpcTalkMorePacket(wire_type, 1).dispatch();
				break;
			}
			break;
		case TalkType::SENDACCEPTDECLINE:
			// Type = 1
			switch (buttonid)
			{
			case Buttons::CLOSE:
				NpcTalkMorePacket(wire_type, -1).dispatch();
				break;
			case Buttons::QNO:
				NpcTalkMorePacket(wire_type, 0).dispatch();
				break;
			case Buttons::QYES:
				NpcTalkMorePacket(wire_type, 1).dispatch();
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
				NpcTalkMorePacket(wire_type, 0).dispatch();
				break;
			case Buttons::OK:
				NpcTalkMorePacket(wire_type, 1).dispatch();
				break;
			}
			break;
		case TalkType::SENDSIMPLE:
			// Type = 4
			switch (buttonid)
			{
			case Buttons::CLOSE:
				NpcTalkMorePacket(wire_type, 0).dispatch();
				break;
			default:
				NpcTalkMorePacket(0).dispatch(); // TODO: Selection
				break;
			}
			break;
		default:
			// An unrecognised type still has to be ENDED.
			//
			// This is what stopped you talking to an NPC a second time.
			// Closing an unknown dialogue matched no case above and so sent
			// nothing at all, while the server went on believing the
			// conversation was open - and it will not begin a new one until
			// the old one is finished. The window shut, and the NPC was mute
			// from then on.
			//
			// Whatever the message was, End Chat means end it.
			if (buttonid == Buttons::CLOSE)
				NpcTalkMorePacket(wire_type, -1).dispatch();

			break;
		}

		return Button::State::NORMAL;
	}

	void UINpcTalk::send_scroll(double yoffset)
	{
		if (slider.isenabled())
			slider.send_scroll(yoffset);
	}

	Cursor::State UINpcTalk::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Point<int16_t> cursor_relative = cursorpos - position;

		// The tap that answered the LAST page is often still down when this
		// one appears. Nothing counts until the pointer has been let go.
		if (!clicked)
		{
			saw_release = true;
		}

		bool may_click = clicked && saw_release && settle <= 0;

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

				if (may_click)
				{
					int32_t chosen = selections[i].index;

					saw_release = false;
					deactivate();
					NpcTalkMorePacket(chosen).dispatch();

					return Cursor::State::CLICKING;
				}

				return Cursor::State::CANCLICK;
			}
		}

		Cursor::State estate = UIElement::send_cursor(may_click, cursorpos);

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

	void UINpcTalk::send_action(Action action)
	{
		// Each of these is a different byte on the wire, not a different way
		// of saying the same thing:
		//
		//   CONFIRM -> 1   yes, next, ok, accept
		//   DENY    -> 0   no, decline
		//   BACK    -> 0   but the PREVIOUS page, which is a different button
		//   CLOSE   -> -1  end the conversation
		//
		// button_pressed already maps each button to the right byte, so this
		// only has to choose the button the message actually offers. Asking
		// for one it does not have would answer a question nobody asked.
		switch (action)
		{
		case Action::CLOSE:
			button_pressed(Buttons::CLOSE);
			break;

		case Action::BACK:
			// Only a two-page message has a previous page. Anywhere else,
			// "back" is the same request as ending the conversation, which is
			// what a person pressing it while a single box is open means.
			if (type == TalkType::SENDNEXTPREV || type == TalkType::SENDPREV)
				button_pressed(Buttons::PREV);
			else
				button_pressed(Buttons::CLOSE);
			break;

		case Action::DENY:
			// Only the two question forms have a "no" to give. On a message
			// that is merely telling you something there is nothing to
			// decline, so this ends it instead of sending an answer the
			// server is not waiting for.
			if (type == TalkType::SENDYESNO)
				button_pressed(Buttons::NO);
			else if (type == TalkType::SENDACCEPTDECLINE)
				button_pressed(Buttons::QNO);
			else
				button_pressed(Buttons::CLOSE);
			break;

		case Action::CONFIRM:
			switch (type)
			{
			case TalkType::SENDOK:
				button_pressed(Buttons::OK);
				break;
			case TalkType::SENDNEXT:
			case TalkType::SENDNEXTPREV:
				button_pressed(Buttons::NEXT);
				break;
			case TalkType::SENDYESNO:
				button_pressed(Buttons::YES);
				break;
			case TalkType::SENDACCEPTDECLINE:
				button_pressed(Buttons::QYES);
				break;
			default:
				break;
			}
			break;
		}
	}

	void UINpcTalk::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (!pressed)
			return;

		if (escape)
		{
			deactivate();

			NpcTalkMorePacket(wire_type, 0).dispatch();

			return;
		}

		// Enter presses whatever this dialogue's affirmative button is.
		//
		// Only escape was handled, so a conversation could be abandoned from
		// the keyboard but never advanced - every "next" and every "yes" had
		// to be clicked. That is merely awkward with a mouse and impossible on
		// a headset held in two hands, which is what made it worth fixing now.
		//
		// Which button that is depends on the message: OK ends a plain one,
		// NEXT turns a page, YES and QYES answer the two kinds of question.
		// button_pressed already knows what each of them means, so this only
		// has to name the right one.
		if (keycode == KeyAction::Id::RETURN)
		{
			switch (type)
			{
			case TalkType::SENDOK:
				button_pressed(Buttons::OK);
				break;
			case TalkType::SENDNEXT:
			case TalkType::SENDNEXTPREV:
				button_pressed(Buttons::NEXT);
				break;
			case TalkType::SENDYESNO:
				button_pressed(Buttons::YES);
				break;
			case TalkType::SENDACCEPTDECLINE:
				button_pressed(Buttons::QYES);
				break;
			default:
				break;
			}
		}
	}

	UIElement::Type UINpcTalk::get_type() const
	{
		return TYPE;
	}

	UINpcTalk::TalkType UINpcTalk::layout_for(int8_t msgtype, int16_t style)
	{
		switch (msgtype)
		{
		case 0:
		{
			// The two bytes after the text are [has-prev][has-next], read as
			// a little-endian short - so the low byte is prev and the high
			// byte is next. Without them every one of these looked like a
			// plain OK, which is why multi-page dialogue never advanced.
			bool prev = (style & 0xFF) != 0;
			bool next = ((style >> 8) & 0xFF) != 0;

			if (prev && next)
				return TalkType::SENDNEXTPREV;

			if (next)
				return TalkType::SENDNEXT;

			if (prev)
				return TalkType::SENDPREV;

			return TalkType::SENDOK;
		}
		case 1:
			return TalkType::SENDYESNO;
		case 4:
			return TalkType::SENDSIMPLE;
		case 12:
			return TalkType::SENDACCEPTDECLINE;
		default:
			return TalkType::NONE;
		}
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
				// EVERY control character, not just carriage return.
				//
				// The font has no glyph for any of them and draws a box, and the
				// server sends more than a carriage return - nulls and the odd
				// vertical tab turn up in quest text too. A newline survives
				// because the layout understands it, and so does a tab.
				//
				// Written for carriage return alone, this left Roger's reward
				// page showing two boxes where the blank line should have been.
				unsigned char raw = static_cast<unsigned char>(tx[i]);

				if (raw >= 0x20 || tx[i] == '\n' || tx[i] == '\t')
					emit(std::string(1, tx[i]));

				i++;
				continue;
			}

			char code = tx[i + 1];

			// Codes that read a number up to a closing '#'.
			if (code == 'p' || code == 't' || code == 'z' || code == 'm' || code == 'o')
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
				case 'o':
					emit(nl::nx::string["Mob.img"][std::to_string(id)]["name"]);
					break;
				default:
				{
					// ITEM NAMES THROUGH ItemData, not one string file.
					//
					// This read Consume.img directly, so it could name the
					// potions a quest hands out and drew a blank for an
					// equip, a scroll or an etc item - which is most of what
					// quests hand out. ItemData already knows which file an
					// id lives in.
					const ItemData& item = ItemData::get(id);

					emit(item.is_valid()
						? item.get_name()
						: ("Item " + std::to_string(id)));

					break;
				}
				}

				i = close + 1;
				continue;
			}

			// A choice opens with #L<n># and closes with #l - EXCEPT WHEN IT
			// DOES NOT.
			//
			// Quest 1036 lists four jobs and closes only the first two:
			//
			//   #L0# Warrior - Perion #l
			//   #L1# Magician - Ellinia #l
			//   #L2# Bowman - Henesys          <- no #l
			//   #L3# Thief - Nautilus          <- no #l
			//
			// Opening a choice used to clear the buffer, so "Bowman -
			// Henesys" was thrown away the instant #L3 arrived, and only
			// three rows were drawn. The end-of-message flush saved the last
			// one, which is why exactly one went missing rather than two.
			//
			// That is far worse than a missing line. The answer to this
			// question is index 3, so the third row on screen sent 2 - and
			// every answer was wrong, on a quiz that cannot be passed.
			//
			// A new choice therefore CLOSES the one before it. The next
			// marker is as good an end as #l, and this data proves the game
			// treated it that way.
			if (code == 'L')
			{
				size_t close = tx.find('#', i + 2);

				if (close != std::string::npos)
				{
					if (in_selection && !selection_text.empty())
					{
						Selection pending;
						pending.index = selection_index;
						pending.label = Text(Text::Font::A13M, Text::Alignment::LEFT,
							Color::Name::BLUE, selection_text);

						selections.push_back(std::move(pending));
					}

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
					sel.label = Text(Text::Font::A13M, Text::Alignment::LEFT,
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

			// THE INLINE PICTURES.
			//
			// #v and #i are item icons, #q a quest icon, #s a skill icon.
			// These are passed through UNCHANGED so the layout sees the macro
			// and reserves a square for it - see LayoutBuilder::add - and
			// draw_inline_icons paints the bitmap into that square.
			//
			// A picture cannot be emitted as text, so this is the one macro
			// kind that survives formatting.
			if (code == 'v' || code == 'i' || code == 'q' || code == 's')
			{
				size_t close = tx.find('#', i + 2);

				if (close != std::string::npos)
				{
					emit(tx.substr(i, close - i + 1));
					i = close + 1;
					continue;
				}
			}

			// #f<path># is a bitmap named by NX PATH, not by id - Roger's
			// reward banner is "#fUI/UIWindow.img/QuestIcon/4/0#". There is
			// no number to reserve a slot against, so it is dropped whole.
			// A gap is honest; the path spilled into the sentence was not.
			if (code == 'f')
			{
				size_t close = tx.find('#', i + 2);

				if (close != std::string::npos)
				{
					i = close + 1;
					continue;
				}
			}

			// Colour and style switches, which carry no text of their own.
			//
			// ⚠ f, v AND z WERE IN THIS LIST AND ARE NOT COLOURS. Treated as
			// two-character colour codes, only the "#v" was eaten and the
			// rest spilled into the dialogue as text. Roger's reward page
			// read "...my friend!??UI/UIWindow.img/QuestIcon/4/0??2010000 3
			// Apple" - the path, the raw ids and all.
			if (std::string("bkrgdenc").find(code) != std::string::npos)
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
			sel.label = Text(Text::Font::A13M, Text::Alignment::LEFT,
				Color::Name::BLUE, selection_text);
			selections.push_back(std::move(sel));
		}

		return out;
	}

	void UINpcTalk::change_text(int32_t npcid, int8_t msgtype, int16_t style, int8_t speakerbyte, const std::string& tx)
	{
		wire_type = msgtype;
		type = layout_for(msgtype, style);

		timestep = 0;
		draw_text = true;
		formatted_text_pos = 0;

		// A NEW PAGE DEMANDS A NEW PRESS. Whatever tap brought this message up
		// may still be down; it answered the last question and must not
		// answer this one too.
		saw_release = false;
		settle = SETTLE_FRAMES;
		formatted_text = format_text(tx, npcid);

		// A SIZE UP. A13M, not A12M.
		//
		// This is read at arm's length on a handheld, by children, and it is
		// the one thing in the window that has to be read rather than
		// recognised. The wrap width goes with it: the same 320 at a larger
		// face would fit fewer words per line and make the box taller for no
		// gain, and the frame is 500 wide.
		text = Text(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::DARKGREY, formatted_text, 350);

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

		// THE BOX HAS TO FIT THE CHOICES, NOT JUST THE MESSAGE.
		//
		// This measured the TEXT only, and draw() then put the choices
		// underneath it - outside the frame, unclipped. An NPC offering
		// eighteen questions drew them from inside the box straight down over
		// the minimap, the HP bar and off the bottom of the screen. Nothing
		// looked broken from the code's point of view: every row was laid out
		// exactly where it was asked to go.
		content_height = static_cast<int16_t>(text_height + selections_height());

		// THE BOX HAS TO BE BIGGER THAN ITS CONTENTS.
		//
		// `height` is the FILL between the frame's top and bottom pieces, and
		// the readable area inside it is smaller still: the body starts
		// BODY_TOP down from the frame's origin, and draw() holds back the
		// last 18 pixels for the frame's lower edge.
		//
		// Sizing the box to the text alone therefore made it a line and a bit
		// too short every time, and the last row was clipped away - which
		// looks exactly like the server having sent less than it did. So the
		// chrome is measured and added, from the same numbers draw() uses
		// rather than a constant that has to be kept in step by hand.
		int16_t chrome = static_cast<int16_t>(BODY_TOP + 19 - top.height());
		int16_t needed = static_cast<int16_t>(content_height + chrome);

		height = min_height;
		show_slider = false;
		scroll = 0;
		offset = 0;

		if (needed > height)
		{
			if (needed > MAX_HEIGHT)
			{
				height = MAX_HEIGHT;
				show_slider = true;

				// In pixels now rather than 400 at a time. That step was fine
				// for a wall of text, where a screenful is the useful unit, and
				// useless for a list where one notch has to land between rows.
				// What cannot be seen at once, in pixels. Measured against
				// the readable area rather than the box, or the last rows
				// stay out of reach no matter how far the slider is dragged.
				int16_t overflow = static_cast<int16_t>(needed - height + 20);

				unitrows = 1;
				rowmax = static_cast<int16_t>(overflow / SCROLL_STEP + 1);

				int16_t slider_y = top.height() - 7;
				slider = Slider(Slider::Type::DEFAULT, Range<int16_t>(slider_y, slider_y + height - 20), top.width() - 26, unitrows, rowmax, onmoved);
			}
			else
			{
				height = needed;
			}
		}

		for (auto& button : buttons)
		{
			button.second->set_active(false);
			button.second->set_state(Button::State::NORMAL);
		}

		int16_t y_cord = height + 48;

		// TWO BUTTONS THAT CANNOT TOUCH.
		//
		// The pairs were placed 65 pixels apart, which was measured when the
		// artwork was drawn at its own size. Every button in this window is
		// scaled by BUTTON_SCALE now - they had to be, for a thumb - so 65 is
		// narrower than the buttons themselves and Accept sat across Decline.
		//
		// Asked of the button rather than worked out from the scale: bounds()
		// already honours set_scale, so this stays right if the scale changes
		// again. Laid out from the RIGHT EDGE, where the single OK sits, so a
		// wider pair grows leftward into empty space instead of off the end
		// of the window.
		auto place_pair = [&](Buttons first, Buttons second, int16_t y)
		{
			constexpr int16_t GAP = 10;
			constexpr int16_t RIGHT = 471;

			int16_t w = buttons[second]->bounds(Point<int16_t>(0, 0)).width();

			buttons[second]->set_position(Point<int16_t>(RIGHT, y));
			buttons[second]->set_active(true);

			buttons[first]->set_position(
				Point<int16_t>(static_cast<int16_t>(RIGHT - w - GAP), y));
			buttons[first]->set_active(true);
		};


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
			place_pair(Buttons::YES, Buttons::NO, y_cord);
			break;
		}
		// These three had their buttons built in the constructor, wired up in
		// button_pressed, and then never positioned or activated - so the
		// dialogue drew with nothing on it but Close.
		//
		// It made every quest that asks you to accept it impossible to accept:
		// the Cygnus Knight start quest from Kimu offered End Chat and nothing
		// else, which ends the character before it begins. Multi-page NPC text
		// was stuck on its first page for the same reason.
		//
		// The two layouts here are the ones already measured for the types
		// that worked - a single button on the right where OK sits, and a pair
		// starting where YES sits - rather than new coordinates guessed at.
		case TalkType::SENDACCEPTDECLINE:
		{
			place_pair(Buttons::QYES, Buttons::QNO, y_cord);
			break;
		}
		case TalkType::SENDNEXT:
			buttons[Buttons::NEXT]->set_position(Point<int16_t>(471, y_cord));
			buttons[Buttons::NEXT]->set_active(true);
			break;
		case TalkType::SENDPREV:
		{
			// A PAGE YOU CAN ONLY GO BACK FROM IS A DEAD END.
			//
			// The server marks the last page of a conversation prev-but-not-
			// next, and this used to render it literally: one Back button and
			// Close. But the SCRIPT is not finished - its next step is the
			// one that hands over the reward and completes the quest, and it
			// only runs on a forward answer (mode 1). There was no button
			// that sent one.
			//
			// Roger's quest 1021 is exactly this. "I will give you a
			// present", then "this is all I can teach you" with the item list
			// on it - and then Back, forever, with the potions never handed
			// over and the quest never completing.
			//
			// OpenStory sidesteps it by throwing the prev/next bytes away
			// entirely and giving every text page a single OK that goes
			// forward. This keeps Back, which is nicer, and adds the forward
			// button that has to exist.
			place_pair(Buttons::PREV, Buttons::OK, y_cord);
			break;
		}
		case TalkType::SENDNEXTPREV:
		{
			place_pair(Buttons::PREV, Buttons::NEXT, y_cord);
			break;
		}
		// Left alone deliberately. SIMPLE is a list of clickable text lines
		// rather than buttons, and the two GET types need a text field this
		// dialogue does not have yet.
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