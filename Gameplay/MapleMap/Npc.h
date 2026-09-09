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

#include "MapObject.h"

#include "../Graphics/Animation.h"
#include "../Graphics/Text.h"
#include "../Util/Randomizer.h"

namespace ms
{
	// Represents a NPC on the current map.
	// Implements the 'Mapobject' interface to be used in a 'Mapobjects' template.
	class Npc : public MapObject
	{
	public:
		// Constructs an NPC by combining data from game files with
		// data sent by the server.
		Npc(int32_t npcid, int32_t oid, bool mirrored, uint16_t fhid, bool control, Point<int16_t> position);

		// Draws the current animation and name/function tags.
		void draw(double viewx, double viewy, float alpha) const override;
		// Updates the current animation and physics.
		int8_t update(const Physics& physics) override;

		// Changes stance and resets animation.
		void set_stance(const std::string& stance);

		// Check whether this is a server-sided NPC.
		bool isscripted() const;
		// Check if the NPC is in range of the cursor.
		bool inrange(Point<int16_t> cursorpos, Point<int16_t> viewpos) const;

		// The middle of the click box, in map coordinates. Overlapping NPCs
		// are settled by whichever centre is nearest the tap - see
		// MapNpcs::send_cursor.
		Point<int16_t> get_click_centre() const;

		// Returns the NPC name.
		std::string get_name();
		// Returns the NPC's function description or title.
		std::string get_func();

		// What this NPC has to offer, drawn as a balloon over their head.
		//
		// The marker is decided entirely on this side: the client reads
		// Quest.nx, checks the requirements against the character, and puts
		// the balloon up itself. The server is never asked and never told.
		enum class QuestMark : uint8_t
		{
			NONE,
			// A quest that can be taken now.
			AVAILABLE,
			// One already taken whose requirements are now met.
			COMPLETABLE
		};

		void set_quest_mark(QuestMark mark);

		// Step the shared quest balloons. Called ONCE a frame by MapNpcs, not
		// per NPC - see the note where it is defined.
		static void update_markers();
		int32_t get_npcid() const;

	private:
		std::map<std::string, Animation> animations;
		std::map<std::string, std::vector<std::string>> lines;
		std::vector<std::string> states;
		std::string name;
		std::string func;
		bool hidename;
		bool scripted;
		bool mouseonly;

		int32_t npcid;
		QuestMark questmark = QuestMark::NONE;
		bool flip;
		std::string stance;

		// ⚠ THE CLICK RECTANGLE THE DATA GIVES, relative to the NPC's origin.
		//
		// Npc.img/<id>/info carries dcLeft, dcRight, dcTop and dcBottom -
		// the box the real client tests a click against. It is NOT the size
		// of the artwork and must not be inferred from it: Empress Cygnus's
		// only animation frame is ONE PIXEL WIDE, a placeholder standing in
		// for the 129x86 `default` bitmap she is actually drawn from. Sizing
		// her hit box from that frame produced a single-pixel column, so she
		// drew perfectly and could not be clicked at all.
		//
		// Zero on all four means the data gave none and inrange() falls back
		// to the animation, which is right for the NPCs that have no dc box.
		int16_t dc_left = 0;
		int16_t dc_right = 0;
		int16_t dc_top = 0;
		int16_t dc_bottom = 0;

		// ⚠ THE PICTURE, WHERE THE ANIMATION IS ONLY A PLACEHOLDER.
		//
		// 33 of the game's 1620 NPCs - Cygnus and Shinsoo among them - have a
		// `stand` frame one pixel wide standing in for a real portrait held at
		// info/default. Drawing the animation renders them as a hairline
		// sliver: the name label and the quest balloon appear over apparently
		// empty ground, which is exactly what they looked like.
		//
		// Invalid for every other NPC, whose animation is the real artwork.
		Texture default_art;
		bool control;

		Randomizer random;
		Text namelabel;
		Text funclabel;
	};
}