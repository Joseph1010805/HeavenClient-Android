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

#include <deque>
#include "../Messages.h"

#include "../Components/Textfield.h"
#include "../Components/Slider.h"

namespace ms
{
	class UIChatbar : public UIDragElement<PosCHAT>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::CHATBAR;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		enum LineType
		{
			UNK0,
			WHITE,
			RED,
			BLUE,
			YELLOW
		};

		UIChatbar();

		void draw(float inter) const override;
		void update() override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;

		bool is_in_range(Point<int16_t> cursorpos) const override;
		Cursor::State send_cursor(bool clicking, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		Cursor::State check_dragtop(bool clicking, Point<int16_t> cursorpos);

		void send_chatline(const std::string& line, LineType type);

		// THE LAST THINGS SAID, for anything that is not this window.
		//
		// The panel's Messages page needs the conversation, and cannot get it
		// from rowtexts: those are laid out against the chat window's own
		// artwork, keyed by a row number that only means something inside its
		// scroll. This is the plain text, newest last, capped.
		//
		// Static because the panel reads it whether or not a chat bar has been
		// built - at the login screen there is no UI element to ask.
		struct Line
		{
			std::string text;
			LineType type;
		};

		static const std::deque<Line>& history();

		static constexpr size_t HISTORY_MAX = 80;

		// Who last whispered us, so /r can answer without retyping a name.
		// Typing an exact character name on a handheld is the whole reason
		// whisper would otherwise go unused.
		void set_last_whisperer(const std::string& name);
		// Acts on a typed party command. False means it was ordinary chat.
		bool handle_command(const std::string& line);
		void display_message(Messages::Type line, UIChatbar::LineType type);
		void toggle_chat();
		void toggle_chat(bool chat_open);
		void toggle_chatfield();
		void toggle_chatfield(bool chatfield_open);
		// Begin listening, and put whatever is heard into the chat box.
		//
		// Public so the SPEAK button on the status bar can start it: that button
		// is on the main screen and always visible, where the chat bar's own
		// microphone only exists while the chat is open.
		// LISTEN, AND SEND WHAT WAS HEARD.
		//
		// `to_world` picks WHERE it lands, not how it is captured - the
		// capture, the live balloon and the end-of-sentence detection are
		// identical either way:
		//
		//   false - map chat. A bubble over the speaker's head, and a line in
		//           the running chat, which is what R3 does.
		//   true  - a super megaphone. A banner across every screen in the
		//           world, which is what L3 does.
		//
		// A handheld has no keyboard, so voice is the only way either of these
		// gets used in practice - the difference between them is REACH.
		void start_dictation(bool to_world = false);

		// THE SAME CAPTURE, ADDRESSED TO ONE PERSON WHO IS NOT HERE.
		//
		// What the Messages page does. The words go into the post box rather
		// than onto the map: if they are online it reaches them in moments,
		// and if they are not it waits in the outbox and lands in their chat
		// whenever they next connect - from another state, if a relay is set.
		void start_dictation_post(const std::string& to);

		// SAY A LINE THAT WAS TYPED SOMEWHERE ELSE.
		//
		// The lower panel has its own keyboard and its own chat page, and it
		// must not reach in and drive this window's textfield to speak - that
		// would mean opening the chat box on the top screen, focusing it,
		// filling it and pressing enter, four steps of pantomime to send one
		// sentence.
		//
		// This is the same road the enter key takes: slash commands are
		// handled, everything else goes out as ordinary chat.
		void say(std::string line);

		bool is_chatopen();
		bool is_chatfieldopen();

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		bool indragrange(Point<int16_t> cursorpos) const override;

		int16_t getchattop(bool chat_open) const;
		int16_t getchatbarheight() const;
		Rectangle<int16_t> getbounds(Point<int16_t> additional_area) const;

		static constexpr int16_t CHATROWHEIGHT = 13;
		static constexpr int16_t MINCHATROWS = 1;
		static constexpr int16_t MAXCHATROWS = 16;
		static constexpr int16_t DIMENSION_Y = 17;
		static constexpr time_t MESSAGE_COOLDOWN = 1'000;

		enum Buttons : uint16_t
		{
			BT_OPENCHAT,
			BT_CLOSECHAT,
			BT_CHAT,
			BT_HELP,
			BT_LINK,
			BT_TAB_0,
			BT_TAB_1,
			BT_TAB_2,
			BT_TAB_3,
			BT_TAB_4,
			BT_TAB_5,
			BT_TAB_ADD,
			BT_CHAT_TARGET,

			// Opens the megaphone. Megaphones used to be something you bought
			// and used up; here they are a button, so shouting to the channel
			// costs nothing and lives next to the thing it is a louder version
			// of.
			BT_MEGA,

			// Speak instead of typing. None of these machines has a keyboard,
			// and spelling a sentence out with a thumbstick on an on-screen
			// key grid is miserable. Only shown when there is a recogniser
			// behind it.
			BT_MIC
		};

		enum ChatTab
		{
			CHT_ALL,
			CHT_BATTLE,
			CHT_PARTY,
			CHT_FRIEND,
			CHT_GUILD,
			CHT_ALLIANCE,
			NUM_CHATTAB
		};

		std::vector<std::string> ChatTabText =
		{
			"All",
			"Battle",
			"Party",
			"Friend",
			"Guild",
			"Alliance"
		};

		bool chatopen;
		bool chatopen_persist;
		bool chatfieldopen;

		// Waiting on the recogniser. Polled in update() rather than
		// pushed to, because Vosk finishes on its own thread and a
		// Textfield is not something to touch from there.
		bool listening;

		// Where the sentence being dictated is bound for - map chat, or a
		// world banner. See start_dictation.
		bool dictate_to_world = false;

		// Who the sentence is addressed to. Empty means the map, which is
		// every case except the Messages page.
		std::string dictate_to;

		// The spoken sentence as it currently stands, and how long it has gone
		// WITHOUT CHANGING. A pause is what ends a spoken line - see update().
		std::string dictated;
		std::string last_whisperer;
		int32_t dictation_quiet;

		// How long a silence means "finished". Long enough to think in the
		// middle of a sentence, short enough that it does not feel as though
		// the game has stopped listening.
		static constexpr int32_t DICTATION_PAUSE = 1500;

		Texture chatspace[4];
		Texture chatenter;
		Texture chatcover;
		Textfield chatfield;
		Point<int16_t> closechat;

		Text chattab_text[UIChatbar::ChatTab::NUM_CHATTAB];
		int16_t chattab_x;
		int16_t chattab_y;
		int16_t chattab_span;

		Slider slider;

		EnumMap<Messages::Type, time_t> message_cooldowns;
		std::vector<std::string> lastentered;
		size_t lastpos;

		int16_t chatrows;
		int16_t rowpos;
		int16_t rowmax;
		std::unordered_map<int16_t, Text> rowtexts;

		bool dragchattop;
	};
}