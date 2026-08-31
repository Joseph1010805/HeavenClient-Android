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
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../UIDragElement.h"

#include "../Components/MapleFrame.h"

#include "../../Graphics/Text.h"

#include "../../Character/Party.h"

namespace ms
{
	// Small always-on party panel (quest-helper style): leader's party title,
	// then one row per member with name and HP gauge. Built from the v83
	// UserList/Party/PartyHP 9-slice + GaugeBar sprites. Hidden when not in
	// a party. Also pushes per-member HP onto the map characters so a gauge
	// floats above each party member's head.
	class UIPartyHUD : public UIDragElement<PosPARTYHUD>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::PARTYHUD;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIPartyHUD();

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		// Open or close the invite list - the panel's second half, which shows
		// everyone standing on this map who is not already in the party. The
		// status bar's party button drives this.
		//
		// Tapping a name beats typing one: the alternative is /invite <name>
		// spelled correctly on a handheld, which is the whole reason parties
		// never got used.
		void toggle_invites();
		bool invites_open() const;

	private:
		static constexpr int16_t WIDTH = 150;
		static constexpr int16_t TITLE_H = 23;
		static constexpr int16_t ROW_H = 31;
		static constexpr int16_t INVITE_HEAD_H = 18;
		static constexpr int16_t INVITE_ROW_H = 17;

		MapleFrame frame;
		Texture gauge_bar, gauge_fill, gauge_grad;
		Texture kick_btn;

		// leader-only per-row kick X hitboxes, refreshed each draw
		struct KickHit
		{
			int32_t cid;
			std::string name;
			Rectangle<int16_t> rect;
		};
		mutable std::vector<KickHit> kick_hits;
		Texture leader_star;

		// The invite half. `nearby` is rebuilt each update() from the map's
		// characters so it cannot go stale as people walk in and out; the
		// hitboxes are rebuilt each draw() alongside the rows they belong to.
		bool show_invites = false;
		std::vector<std::pair<int32_t, std::string>> nearby;
		mutable std::vector<KickHit> invite_hits;
		mutable Rectangle<int16_t> leave_rect;

		mutable Text title;
		mutable Text member_name;
		mutable Text member_hp_text;

		// cids we last stamped overhead gauges onto, so members who leave
		// get their gauge cleared
		std::vector<int32_t> stamped_cids;
	};
}
