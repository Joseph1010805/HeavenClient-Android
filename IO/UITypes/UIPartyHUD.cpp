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
//
// From the OpenStory fork (https://github.com/rdiol12/OpenStory), AGPL-3.0,
// used with thanks. The sprite paths and the pixel offsets into them were
// worked out there against the v83 data.
//
#include "UIPartyHUD.h"

#include "UINotice.h"

#include "../UI.h"

#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/GameplayPackets.h"

#include "../../Graphics/Geometry.h"

#include <nlnx/nx.hpp>

#include <algorithm>

namespace ms
{
	UIPartyHUD::UIPartyHUD() : UIDragElement<PosPARTYHUD>(Point<int16_t>(WIDTH, 20))
	{
		nl::node PartyHP = nl::nx::ui["UIWindow.img"]["UserList"]["Party"]["PartyHP"];
		nl::node Gauge = PartyHP["GaugeBar"];

		// panel backdrop = the item-tooltip 9-slice frame
		frame = MapleFrame(nl::nx::ui["UIToolTip.img"]["Item"]["Frame2"]);

		gauge_bar = Gauge["bar"];
		gauge_fill = Gauge["gauge"];
		gauge_grad = Gauge["graduation"];

		leader_star = nl::nx::ui["UIWindow2.img"]["UserList"]["Main"]["Party"]["PartySearch"]["PartyInfo"]["leader"];

		// same X as the quest helper's per-quest close button
		nl::node btdel = nl::nx::ui["UIWindow2.img"]["QuestAlarm"]["BtDelete"];
		if (!btdel)
			btdel = nl::nx::ui["UIWindow.img"]["QuestAlarm"]["BtDelete"];
		kick_btn = Texture(btdel["normal"]["0"]);

		title = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::WHITE);
		member_name = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::WHITE);
		member_hp_text = Text(Text::Font::A11B, Text::Alignment::RIGHT, Color::Name::WHITE);

		dimension = Point<int16_t>(WIDTH, TITLE_H + 14);
	}

	void UIPartyHUD::draw(float inter) const
	{
		const Party& party = Stage::get().get_player().get_party();

		if (!party.is_in_party())
			return;

		const auto& members = party.get_members();
		int16_t rows = static_cast<int16_t>(members.size());

		// A PARTY OF ONE IS NOT A PARTY TO LOOK AT.
		//
		// The server will happily hold you in a party by yourself - inviting
		// somebody creates one before they answer, and leaving the last other
		// member leaves you in it. That is correct server-side and it is why
		// "JosephGrey's Party" was on screen for somebody who had never made
		// one on purpose.
		//
		// Nothing is changed about the party itself: the Party page still
		// shows it and still offers to leave. This is only about not putting
		// a roster of one person on top of the game.
		// ...UNLESS THE INVITE LIST IS OPEN.
		//
		// Hiding a roster of one is right; hiding the thing that BUILDS the
		// roster is not. With no party there are no rows, so this returned
		// before drawing anything at all - and the invite list, the only way
		// to start a party, was drawn by the code below. Opening it did
		// nothing visible whatsoever.
		if (rows < 2 && !show_invites)
			return;

		// The invite half adds a heading, a row per nearby player and the
		// leave row, and the frame has to be sized for all of it BEFORE it is
		// drawn - so this is counted up here rather than as we go.
		int16_t invite_rows = show_invites
			? static_cast<int16_t>(nearby.size() + 1) : 0;
		int16_t invite_h = show_invites
			? static_cast<int16_t>(INVITE_HEAD_H + invite_rows * INVITE_ROW_H) : 0;

		int16_t inner_h = static_cast<int16_t>(TITLE_H + rows * ROW_H + invite_h);

		int16_t top_h = 7;
		int16_t panel_h = static_cast<int16_t>(top_h + inner_h + 10);

		Point<int16_t> tl = position;

		frame.draw(tl + Point<int16_t>(WIDTH / 2, panel_h - 6), WIDTH - 19, panel_h - 17);

		// title: the leader's party
		std::string leader_name;
		for (const auto& m : members)
			if (m.cid == party.get_leader())
				leader_name = m.name;

		title.change_text(leader_name.empty() ? "Party" : leader_name + "'s Party");
		title.draw(tl + Point<int16_t>(10, top_h + 1));

		// white divider under the title, before the gauges
		static const ColorBox divider(WIDTH - 10, 1, Color::Name::WHITE, 0.7f);
		divider.draw(DrawArgument(tl + Point<int16_t>(5, top_h + TITLE_H + 3)));

		int32_t my_cid = Stage::get().get_player().get_oid();
		bool i_lead = (my_cid == party.get_leader());
		kick_hits.clear();

		for (int16_t i = 0; i < rows; i++)
		{
			const PartyMember& m = members[i];
			int16_t row_y = static_cast<int16_t>(top_h + TITLE_H + 11 + i * ROW_H);

			if (m.cid == party.get_leader())
				leader_star.draw(DrawArgument(tl + Point<int16_t>(4, row_y + 7)));

			member_name.change_text(m.name);
			// row order: gauge on top, 2px, then the name
			member_name.draw(tl + Point<int16_t>(15, row_y + 6));

			// HP for self comes live from our stats; others from party updates
			int32_t hp = m.hp, maxhp = m.maxhp;
			if (m.cid == my_cid)
			{
				const CharStats& stats = Stage::get().get_player().get_stats();
				hp = stats.get_stat(Maplestat::Id::HP);
				maxhp = stats.get_total(Equipstat::Id::HP);
			}

			// gauge: bar frame + fill scaled by hp ratio (bar origin -3,-3)
			Point<int16_t> bar_at = tl + Point<int16_t>(15, row_y - 3);
			gauge_bar.draw(DrawArgument(bar_at));

			if (maxhp > 0)
			{
				float ratio = hp > 0 ? static_cast<float>(hp) / static_cast<float>(maxhp) : 0.0f;
				if (ratio > 1.0f)
					ratio = 1.0f;
				// graduation interior track: x 4..64, y 3..9 from its corner
				int16_t fill_w = static_cast<int16_t>(61 * ratio);
				if (fill_w > 0)
					gauge_fill.draw(DrawArgument(
						bar_at + Point<int16_t>(4, 3),
						Point<int16_t>(fill_w, 0)));

				member_hp_text.change_text(std::to_string(hp) + " / " + std::to_string(maxhp));
			}
			else
			{
				member_hp_text.change_text(m.online ? "-" : "offline");
			}

			member_hp_text.draw(tl + Point<int16_t>(WIDTH - 8, row_y - 3));
			gauge_grad.draw(DrawArgument(bar_at));

			// leader-only kick X beside the member's name
			if (i_lead && m.cid != my_cid)
			{
				// right after the gauge bar (bar spans x 15..84)
				Point<int16_t> kx = tl + Point<int16_t>(84, row_y - 5);
				kick_btn.draw(DrawArgument(kx));
				kick_hits.push_back({ m.cid, m.name,
					Rectangle<int16_t>(kx, kx + Point<int16_t>(14, 14)) });
			}
		}

		invite_hits.clear();
		leave_rect = Rectangle<int16_t>();

		if (show_invites)
		{
			int16_t iy = static_cast<int16_t>(top_h + TITLE_H + 11 + rows * ROW_H);

			static const ColorBox rule(WIDTH - 10, 1, Color::Name::WHITE, 0.4f);
			rule.draw(DrawArgument(tl + Point<int16_t>(5, iy)));

			member_name.change_text(nearby.empty()
				? "Nobody else on this map"
				: "Tap a name to invite");
			member_name.draw(tl + Point<int16_t>(10, iy + 3));

			iy = static_cast<int16_t>(iy + INVITE_HEAD_H);

			for (const auto& who : nearby)
			{
				member_name.change_text(who.second);
				member_name.draw(tl + Point<int16_t>(15, iy));

				Point<int16_t> rt = tl + Point<int16_t>(5, iy);
				invite_hits.push_back({ who.first, who.second,
					Rectangle<int16_t>(rt, rt + Point<int16_t>(WIDTH - 10, INVITE_ROW_H)) });

				iy = static_cast<int16_t>(iy + INVITE_ROW_H);
			}

			// Leave sits last, so a mis-tap while inviting does not drop you
			// out of the party you are trying to fill.
			member_hp_text.change_text("Leave party");
			member_hp_text.draw(tl + Point<int16_t>(WIDTH - 8, iy));

			Point<int16_t> lt = tl + Point<int16_t>(5, iy);
			leave_rect = Rectangle<int16_t>(lt, lt + Point<int16_t>(WIDTH - 10, INVITE_ROW_H));
		}

		UIElement::draw(inter);
	}

	void UIPartyHUD::update()
	{
		UIElement::update();

		const Party& party = Stage::get().get_player().get_party();
		Player& player = Stage::get().get_player();
		MapChars& chars = Stage::get().get_chars();

		// overhead gauges: stamp current members, clear anyone who left
		std::vector<int32_t> now;

		if (party.is_in_party())
		{
			int32_t my_cid = player.get_oid();

			for (const auto& m : party.get_members())
			{
				// no overhead bar on yourself — only the rest of the party
				if (m.cid == my_cid)
					continue;

				now.push_back(m.cid);

				if (auto chr = chars.get_char(m.cid))
					chr->set_party_hp(m.hp, m.maxhp);
			}

			// Everyone on this map who is not already with us. Rebuilt every
			// tick so the list cannot offer somebody who has walked off.
			nearby.clear();

			if (show_invites)
			{
				for (auto& entry : *chars.get_chars())
				{
					auto* obj = entry.second.get();

					if (obj == nullptr)
						continue;

					int32_t cid = obj->get_oid();

					bool already = (cid == my_cid);
					for (const auto& m : party.get_members())
						already = already || (m.cid == cid);

					if (already)
						continue;

					std::string name = static_cast<Char*>(obj)->get_name();

					if (!name.empty())
						nearby.emplace_back(cid, name);
				}

				std::sort(nearby.begin(), nearby.end(),
					[](const auto& a, const auto& b) { return a.second < b.second; });
			}

			int16_t rows = static_cast<int16_t>(party.get_members().size());
			int16_t extra = show_invites
				? static_cast<int16_t>(INVITE_HEAD_H + (nearby.size() + 1) * INVITE_ROW_H)
				: 0;

			dimension = Point<int16_t>(WIDTH,
				static_cast<int16_t>(TITLE_H + rows * ROW_H + extra + 14));
		}
		else
		{
			// NOT IN A PARTY IS EXACTLY WHEN YOU NEED THIS LIST.
			//
			// This used to clear the list and force the panel shut whenever
			// you had no party - "there is nothing to invite anyone to" -
			// which made starting one impossible: the only way into a party
			// was to already be in one. Two people standing on the same map
			// simply could not see each other.
			//
			// The server has never needed a party to exist first. Cosmic's
			// PartyOperationHandler case 4 creates one for the inviter on the
			// spot when they have none, so a bare invite is the whole
			// interaction - unlike a TRADE, which really does need its room
			// opening first.
			nearby.clear();

			if (show_invites)
			{
				int32_t my_cid = player.get_oid();

				for (auto& entry : *chars.get_chars())
				{
					auto* obj = entry.second.get();

					if (obj == nullptr || obj->get_oid() == my_cid)
						continue;

					std::string name = static_cast<Char*>(obj)->get_name();

					if (!name.empty())
						nearby.emplace_back(obj->get_oid(), name);
				}

				std::sort(nearby.begin(), nearby.end(),
					[](const auto& a, const auto& b) { return a.second < b.second; });
			}

			// No member rows, but the invite list still needs room, and
			// "Leave party" still takes its line - tapping it with no party
			// is harmless and the server ignores it.
			int16_t extra = show_invites
				? static_cast<int16_t>(INVITE_HEAD_H + (nearby.size() + 1) * INVITE_ROW_H)
				: 0;

			dimension = Point<int16_t>(WIDTH,
				static_cast<int16_t>(TITLE_H + extra + 14));
		}

		for (int32_t cid : stamped_cids)
		{
			if (std::find(now.begin(), now.end(), cid) != now.end())
				continue;

			if (cid == player.get_oid())
				player.clear_party_hp();
			else if (auto chr = chars.get_char(cid))
				chr->clear_party_hp();
		}

		stamped_cids = std::move(now);
	}

	Cursor::State UIPartyHUD::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		for (const auto& hit : kick_hits)
		{
			if (hit.rect.contains(cursorpos))
			{
				if (clicked)
				{
					int32_t cid = hit.cid;
					UI::get().emplace<UIYesNo>(
						"Kick " + hit.name + " from the party?",
						[cid](bool yes)
						{
							if (yes)
								ExpelFromPartyPacket(cid).dispatch();
						});

					return Cursor::State::CLICKING;
				}

				return Cursor::State::CANCLICK;
			}
		}

		for (const auto& hit : invite_hits)
		{
			if (hit.rect.contains(cursorpos))
			{
				if (clicked)
				{
					InviteToPartyPacket(hit.name).dispatch();
					return Cursor::State::CLICKING;
				}

				return Cursor::State::CANCLICK;
			}
		}

		if (leave_rect.contains(cursorpos))
		{
			if (clicked)
			{
				UI::get().emplace<UIYesNo>(
					"Leave the party?",
					[](bool yes)
					{
						if (yes)
							LeavePartyPacket().dispatch();
					});

				return Cursor::State::CLICKING;
			}

			return Cursor::State::CANCLICK;
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIPartyHUD::toggle_invites()
	{
		show_invites = !show_invites;
	}

	bool UIPartyHUD::invites_open() const
	{
		return show_invites;
	}

	UIElement::Type UIPartyHUD::get_type() const
	{
		return TYPE;
	}
}
