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

#include "DrawArgument.h"

#include <map>
#include <vector>

namespace ms
{
	class Text
	{
	public:
		enum Font
		{
			A11M,
			A11B,
			A12M,
			A12B,
			A13M,
			A13B,
			A15B,
			A18M,
			NUM_FONTS
		};

		enum Alignment
		{
			LEFT,
			CENTER,
			RIGHT
		};

		enum Background
		{
			NONE,
			NAMETAG
		};

		class Layout
		{
		public:
			struct Word
			{
				size_t first;
				size_t last;
				Font font;
				Color::Name color;
			};

			struct Line
			{
				std::vector<Word> words;
				Point<int16_t> position;
			};

			// AN INLINE PICTURE, where a `#X<id>#` macro was.
			//
			// Quest text is written with the picture in the sentence -
			// "#v2010000# 3 #t2010000#" is an apple icon, the number three
			// and the word Apple. The layout cannot draw the bitmap itself
			// (it knows about glyphs, not about Item.nx), so it reserves a
			// square of `size` at `pos` and records what belongs there. The
			// caller paints it - see UINpcTalk::draw_inline_icons.
			//
			// Ported from OpenStory, which solved this first.
			enum class ImageKind : int8_t
			{
				ITEM = 0,   // #v<id># or #i<id># - Item.nx
				QUEST = 1,  // #q<id># - UI.nx/UIWindow.img/QuestIcon
				SKILL = 2,  // #s<id># - Skill.nx/<job>.img/skill/<id>/icon
				FACE = 3    // #f<id># - Character.nx/Face/<id>.img
			};

			struct Image
			{
				Point<int16_t> pos;
				int32_t item_id = 0;
				int16_t size = 0;
				ImageKind kind = ImageKind::ITEM;
			};

			Layout(const std::vector<Line>& lines, const std::vector<int16_t>& advances, const std::vector<Image>& images, int16_t width, int16_t height, int16_t endx, int16_t endy);
			Layout();

			int16_t width() const;
			int16_t height() const;
			int16_t advance(size_t index) const;
			Point<int16_t> get_dimensions() const;
			Point<int16_t> get_endoffset() const;
			const std::vector<Image>& get_images() const;

			using iterator = std::vector<Line>::const_iterator;
			iterator begin() const;
			iterator end() const;

		private:
			std::vector<Line> lines;
			std::vector<int16_t> advances;
			std::vector<Image> images;
			Point<int16_t> dimensions;
			Point<int16_t> endoffset;
		};

		Text(Font font, Alignment alignment, Color::Name color, Background background, const std::string& text = "", uint16_t maxwidth = 0, bool formatted = true, int16_t line_adj = 0);
		Text(Font font, Alignment alignment, Color::Name color, const std::string& text = "", uint16_t maxwidth = 0, bool formatted = true, int16_t line_adj = 0);
		Text();

		void draw(const DrawArgument& args) const;
		void draw(const DrawArgument& args, const Range<int16_t>& vertical) const;

		void change_text(const std::string& text);
		void change_color(Color::Name color);
		void set_background(Background background);

		bool empty() const;
		size_t length() const;
		int16_t width() const;
		int16_t height() const;
		uint16_t advance(size_t pos) const;
		Point<int16_t> dimensions() const;
		Point<int16_t> endoffset() const;

		// The inline pictures this text asked for, with their positions
		// already resolved against the laid-out lines. Empty when there are
		// none, which is almost all text.
		const std::vector<Layout::Image>& images() const;
		const std::string& get_text() const;

	private:
		void reset_layout();

		Font font;
		Alignment alignment;
		Color::Name color;
		Background background;
		Layout layout;
		uint16_t maxwidth;
		bool formatted;
		std::string text;
		int16_t line_adj;
	};
}