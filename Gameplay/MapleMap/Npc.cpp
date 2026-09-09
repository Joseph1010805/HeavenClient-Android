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

#include "Npc.h"

#include <nlnx/nx.hpp>

namespace ms
{
	namespace
	{
		// The two balloons, loaded once and shared by every NPC on the map.
		//
		// They come from the v178 UI - v83's has no QuestIcon node - and are
		// animations rather than stills, which is why they are `Animation`
		// and get updated below.
		struct QuestMarkers
		{
			Animation available;
			Animation completable;

			QuestMarkers()
			{
				nl::node icons = nl::nx::ui["UIWindow.img"]["QuestIcon"];

				available = icons["0"];
				completable = icons["2"];
			}
		};

		QuestMarkers& markers()
		{
			// Built on first use rather than at startup: nl::nx is not open
			// yet when statics are initialised.
			static QuestMarkers instance;

			return instance;
		}
	}

	Npc::Npc(int32_t id, int32_t o, bool fl, uint16_t f, bool cnt, Point<int16_t> position) : MapObject(o)
	{
		std::string strid = std::to_string(id);
		strid.insert(0, 7 - strid.size(), '0');
		strid.append(".img");

		nl::node src = nl::nx::npc[strid];
		nl::node strsrc = nl::nx::string["Npc.img"][std::to_string(id)];

		std::string link = src["info"]["link"];

		if (link.size() > 0)
		{
			link.append(".img");
			src = nl::nx::npc[link];
		}

		nl::node info = src["info"];

		// The click box, straight from the data - see the note on dc_left.
		dc_left = static_cast<int16_t>(info["dcLeft"].get_integer());
		dc_right = static_cast<int16_t>(info["dcRight"].get_integer());
		dc_top = static_cast<int16_t>(info["dcTop"].get_integer());
		dc_bottom = static_cast<int16_t>(info["dcBottom"].get_integer());

		hidename = info["hideName"].get_bool();
		mouseonly = info["talkMouseOnly"].get_bool();
		scripted = info["script"].size() > 0 || info["shop"].get_bool();

		for (auto npcnode : src)
		{
			std::string state = npcnode.name();

			if (state != "info")
			{
				animations[state] = npcnode;
				states.push_back(state);
			}

			for (auto speaknode : npcnode["speak"])
				lines[state].push_back(strsrc[speaknode.get_string()]);
		}

		name = std::string(strsrc["name"]);
		func = std::string(strsrc["func"]);

		namelabel = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::YELLOW, Text::Background::NAMETAG, name);
		funclabel = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::YELLOW, Text::Background::NAMETAG, func);

		npcid = id;
		flip = !fl;
		control = cnt;
		stance = "stand";

		// THE PORTRAIT, WHERE THE ANIMATION IS A PLACEHOLDER - see the note on
		// default_art. Eight pixels is well below any real sprite and well
		// above the 1x60 and 4x4 stubs these NPCs actually carry.
		if (!animations.count(stance)
			|| animations.at(stance).get_dimensions().x() < 8)
		{
			nl::node fallback = info["default"];

			if (fallback.data_type() == nl::node::type::bitmap)
				default_art = fallback;
		}

		phobj.fhid = f;
		set_position(position);
	}

	void Npc::draw(double viewx, double viewy, float alpha) const
	{
		Point<int16_t> absp = phobj.get_absolute(viewx, viewy, alpha);

		// The picture first where there is one - see default_art. Without this
		// these NPCs drew as a one-pixel hairline and read as not being there.
		if (default_art.is_valid())
			default_art.draw(DrawArgument(absp, flip));
		else if (animations.count(stance))
			animations.at(stance).draw(DrawArgument(absp, flip), alpha);

		if (!hidename)
		{
			// If ever changing code for namelabel confirm placements with map 10000
			namelabel.draw(absp + Point<int16_t>(0, -4));
			funclabel.draw(absp + Point<int16_t>(0, 18));
		}

		// The balloon sits above the head, clear of the name.
		Point<int16_t> over = absp + Point<int16_t>(0, -68);

		switch (questmark)
		{
		case QuestMark::AVAILABLE:
			markers().available.draw(DrawArgument(over), alpha);
			break;
		case QuestMark::COMPLETABLE:
			markers().completable.draw(DrawArgument(over), alpha);
			break;
		default:
			break;
		}
	}

	void Npc::set_quest_mark(QuestMark mark)
	{
		questmark = mark;
	}

	int32_t Npc::get_npcid() const
	{
		return npcid;
	}

	// ONE SHARED PAIR OF BALLOONS, ADVANCED ONCE A FRAME.
	//
	// This used to live in Npc::update, which runs once per NPC - so the two
	// animations were stepped once for EVERY npc on the map wearing a mark.
	// Eight quest NPCs in a town meant eight steps a frame, and at a 150ms
	// frame delay that is a strobe rather than a bob. It looked like the
	// artwork was wrong; it was the same drawing being run eight times fast.
	//
	// They are shared on purpose - one copy for the whole map rather than a
	// pair per NPC - so the update has to be shared too.
	void Npc::update_markers()
	{
		markers().available.update();
		markers().completable.update();
	}

	int8_t Npc::update(const Physics& physics)
	{
		if (!active)
			return phobj.fhlayer;

		physics.move_object(phobj);

		// ⚠ THE BALLOONS ARE NOT UPDATED HERE. See Npc::update_markers.

		if (animations.count(stance))
		{
			bool aniend = animations.at(stance).update();

			if (aniend && states.size() > 0)
			{
				size_t next_stance = random.next_int(states.size());
				std::string new_stance = states[next_stance];
				set_stance(new_stance);
			}
		}

		return phobj.fhlayer;
	}

	void Npc::set_stance(const std::string& st)
	{
		if (stance != st)
		{
			stance = st;

			auto iter = animations.find(stance);

			if (iter == animations.end())
				return;

			iter->second.reset();
		}
	}

	bool Npc::isscripted() const
	{
		return scripted;
	}

	Point<int16_t> Npc::get_click_centre() const
	{
		Point<int16_t> at = get_position();

		// The same box inrange() tests, so the two can never disagree about
		// which NPC a tap belongs to.
		if (dc_left != dc_right || dc_top != dc_bottom)
			return Point<int16_t>(
				static_cast<int16_t>(at.x() + (dc_left + dc_right) / 2),
				static_cast<int16_t>(at.y() + (dc_top + dc_bottom) / 2));

		Point<int16_t> dim =
			animations.count(stance) ?
			animations.at(stance).get_dimensions() :
			Point<int16_t>();

		return Point<int16_t>(
			at.x(), static_cast<int16_t>(at.y() - dim.y() / 2));
	}

	bool Npc::inrange(Point<int16_t> cursorpos, Point<int16_t> viewpos) const
	{
		if (!active)
			return false;

		Point<int16_t> absp = get_position() + viewpos;

		// ⚠ THE DATA'S OWN CLICK BOX FIRST. The artwork's size is a guess at
		// it and a bad one: an NPC drawn from info/default with a placeholder
		// animation frame - Empress Cygnus's is 1x60 - gets a hit box one
		// pixel wide and becomes permanently unclickable while looking
		// completely normal. dcLeft/dcRight/dcTop/dcBottom are what the real
		// client tests, and they are correct by construction.
		if (dc_left != dc_right || dc_top != dc_bottom)
			return Rectangle<int16_t>(
				static_cast<int16_t>(absp.x() + dc_left),
				static_cast<int16_t>(absp.x() + dc_right),
				static_cast<int16_t>(absp.y() + dc_top),
				static_cast<int16_t>(absp.y() + dc_bottom)
				).contains(cursorpos);

		// Only where the data gave none. Most NPCs are drawn from a real
		// animation and this has always been right for them.
		Point<int16_t> dim =
			animations.count(stance) ?
			animations.at(stance).get_dimensions() :
			Point<int16_t>();

		return Rectangle<int16_t>(
			absp.x() - dim.x() / 2,
			absp.x() + dim.x() / 2,
			absp.y() - dim.y(),
			absp.y()
			).contains(cursorpos);
	}

	std::string Npc::get_name()
	{
		return name;
	}

	std::string Npc::get_func()
	{
		return func;
	}
}