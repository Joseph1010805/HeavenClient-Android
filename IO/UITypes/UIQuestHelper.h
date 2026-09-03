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

#include "../../Graphics/Text.h"

namespace ms
{
	// The on-screen quest tracker: which quests are live and how far along
	// each one is.
	//
	// Entirely client-side. Everything it shows is already known - Questlog
	// holds the started quests and their progress strings, QuestData holds the
	// names and requirements, and UIQuestLog was already doing this arithmetic
	// inside a window nobody keeps open. This just puts it where it can be
	// read while playing.
	class UIQuestHelper : public UIDragElement<PosQUESTHELPER>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::QUESTHELPER;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIQuestHelper();

		void draw(float inter) const override;
		void update() override;

		// A WAY TO SHUT IT.
		//
		// This is a permanent overlay on the top screen with no close box of
		// its own, so on a single-screen build - the RP5, which has no lower
		// panel to move it to - there was no way to get rid of it at all.
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

	private:
		// One line per requirement, so a quest wanting three different
		// monsters costs three rows.
		struct Row
		{
			std::string text;
			bool done;
		};

		struct Entry
		{
			std::string name;
			std::vector<Row> rows;
		};

		// Tracking every quest at once would fill the screen - v83 lets you
		// hold far more than fit. The oldest few are the ones being worked on.
		static constexpr size_t MAX_QUESTS = 5;

		static constexpr int16_t WIDTH = 190;
		static constexpr int16_t TITLE_H = 20;
		static constexpr int16_t NAME_H = 17;
		static constexpr int16_t ROW_H = 15;

		Rectangle<int16_t> close_box() const;

		std::vector<Entry> entries;

		MapleFrame frame;

		mutable Text title;
		mutable Text quest_name;
		mutable Text row_text;
		mutable Text row_done;
	};
}
