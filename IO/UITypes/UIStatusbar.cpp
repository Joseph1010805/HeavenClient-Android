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
#include "UIStatusbar.h"

#include "UIMegaphone.h"
#include "UIChatbar.h"
#include "UIPartyHUD.h"

#include "../../Net/Packets/GameplayPackets.h"

#include "../Components/AreaButton.h"
#include "../../Speech.h"

#include "../UI.h"

#include "../Components/MapleButton.h"
#include "../Gameplay/Stage.h"
#include "../UITypes/UIQuestLog.h"
#include "../UITypes/UIUserList.h"
#include "../UITypes/UIStatsinfo.h"
#include "../UITypes/UISkillbook.h"
#include "../UITypes/UIEquipInventory.h"
#include "../UITypes/UIItemInventory.h"
#include "../UITypes/UIChannel.h"
#include "../UITypes/UIJoypad.h"
#include "../UITypes/UIEvent.h"
#include "../UITypes/UIKeyConfig.h"
#include "../Data/ItemData.h"
#include "../Data/SkillData.h"
#include "../Net/Packets/PlayerPackets.h"
#include "../KeyConfig.h"
#include "../SecondScreen.h"

#include <tuple>
#include "../UITypes/UIChat.h"
#include "../UITypes/UIOptionMenu.h"
#include "../UITypes/UIQuit.h"
#include "../UITypes/UINotice.h"
#include "../UITypes/UILoginNotice.h"
#include "../Character/ExpTable.h"

#include <nlnx/nx.hpp>

namespace ms
{
	UIStatusbar::UIStatusbar(const CharStats& st) : stats(st)
	{
		quickslot_active = false;
		quickslot_adj = Point<int16_t>(QUICKSLOT_MAX, 0);
		VWIDTH = Constants::Constants::get().get_viewwidth();
		VHEIGHT = Constants::Constants::get().get_viewheight();

		menu_active = false;
		setting_active = false;
		community_active = false;
		character_active = false;
		event_active = false;

		std::string stat = "status";

		if (VWIDTH == 800)
			stat += "800";

		nl::node mainBar = nl::nx::ui["StatusBar3.img"]["mainBar"];
		nl::node status = mainBar[stat];
		nl::node EXPBar = mainBar["EXPBar"];
		nl::node EXPBarRes = EXPBar[VWIDTH];
		nl::node menu = mainBar["menu"];
		nl::node quickSlot = mainBar["quickSlot"];
		nl::node submenu = mainBar["submenu"];

		exp_pos = Point<int16_t>(0, 87);

		sprites.emplace_back(EXPBar["backgrnd"], DrawArgument(Point<int16_t>(0, 87), Point<int16_t>(VWIDTH, 0)));
		sprites.emplace_back(EXPBarRes["layer:back"], exp_pos);

		int16_t exp_max = VWIDTH - 16;

		expbar = Gauge(
			EXPBarRes.resolve("layer:gauge"),
			EXPBarRes.resolve("layer:cover"),
			EXPBar.resolve("layer:effect"),
			exp_max, 0.0f
		);

		int16_t pos_adj = 0;

		if (VWIDTH == 1280)
			pos_adj = 87;
		else if (VWIDTH == 1366)
			pos_adj = 171;
		else if (VWIDTH == 1920)
			pos_adj = 448;

		if (VWIDTH == 1024)
			quickslot_min = 1;
		else
			quickslot_min = 0;

		if (VWIDTH == 800)
		{
			// Everything printed ON the panel moves with it. The EXP figure
			// (statset) does not - that belongs to the EXP bar, which is
			// staying where it is.
			hpmp_pos = Point<int16_t>(412 - HPMP_SHIFT, 40);
			hpset_pos = Point<int16_t>(530 - HPMP_SHIFT, 70);
			mpset_pos = Point<int16_t>(528 - HPMP_SHIFT, 86);
			statset_pos = Point<int16_t>(427, 111);
			levelset_pos = Point<int16_t>(461 - HPMP_SHIFT, 48);
			namelabel_pos = Point<int16_t>(487 - HPMP_SHIFT, 40);
			quickslot_pos = Point<int16_t>(579, 0);

			// Menu
			menu_pos = Point<int16_t>(682, -280);
			setting_pos = menu_pos + Point<int16_t>(0, 132);
			community_pos = menu_pos + Point<int16_t>(-26, 196);
			character_pos = menu_pos + Point<int16_t>(-61, 168);
			event_pos = menu_pos + Point<int16_t>(-94, 252);
		}
		else
		{
			hpmp_pos = Point<int16_t>(416 + pos_adj, 40);
			hpset_pos = Point<int16_t>(550 + pos_adj, 70);
			mpset_pos = Point<int16_t>(546 + pos_adj, 86);
			statset_pos = Point<int16_t>(539 + pos_adj, 111);
			levelset_pos = Point<int16_t>(465 + pos_adj, 48);
			namelabel_pos = Point<int16_t>(493 + pos_adj, 40);
			quickslot_pos = Point<int16_t>(628 + pos_adj, 37);

			// Menu
			menu_pos = Point<int16_t>(720 + pos_adj, -280);
			setting_pos = menu_pos + Point<int16_t>(0, 132);
			community_pos = menu_pos + Point<int16_t>(-26, 196);
			character_pos = menu_pos + Point<int16_t>(-61, 168);
			event_pos = menu_pos + Point<int16_t>(-94, 252);
		}

		if (VWIDTH == 1280)
		{
			statset_pos = Point<int16_t>(580 + pos_adj, 111);
			quickslot_pos = Point<int16_t>(622 + pos_adj, 37);

			// Menu
			menu_pos += Point<int16_t>(-7, 0);
			setting_pos += Point<int16_t>(-7, 0);
			community_pos += Point<int16_t>(-7, 0);
			character_pos += Point<int16_t>(-7, 0);
			event_pos += Point<int16_t>(-7, 0);
		}
		else if (VWIDTH == 1366)
		{
			quickslot_pos = Point<int16_t>(623 + pos_adj, 37);

			// Menu
			menu_pos += Point<int16_t>(-5, 0);
			setting_pos += Point<int16_t>(-5, 0);
			community_pos += Point<int16_t>(-5, 0);
			character_pos += Point<int16_t>(-5, 0);
			event_pos += Point<int16_t>(-5, 0);
		}
		else if (VWIDTH == 1920)
		{
			quickslot_pos = Point<int16_t>(900 + pos_adj, 37);

			// Menu
			menu_pos += Point<int16_t>(272, 0);
			setting_pos += Point<int16_t>(272, 0);
			community_pos += Point<int16_t>(272, 0);
			character_pos += Point<int16_t>(272, 0);
			event_pos += Point<int16_t>(272, 0);
		}

		hpmp_sprites.emplace_back(status["backgrnd"], hpmp_pos - Point<int16_t>(1, 0));
		hpmp_sprites.emplace_back(status["layer:cover"], hpmp_pos - Point<int16_t>(1, 0));

		if (VWIDTH == 800)
			hpmp_sprites.emplace_back(status["layer:Lv"], hpmp_pos);
		else
			hpmp_sprites.emplace_back(status["layer:Lv"], hpmp_pos - Point<int16_t>(1, 0));

		int16_t hpmp_max = 139;

		if (VWIDTH > 800)
			hpmp_max += 30;

		hpbar = Gauge(status.resolve("gauge/hp/layer:0"), hpmp_max, 0.0f);
		mpbar = Gauge(status.resolve("gauge/mp/layer:0"), hpmp_max, 0.0f);

		statset = Charset(EXPBar["number"], Charset::Alignment::RIGHT);
		hpmpset = Charset(status["gauge"]["number"], Charset::Alignment::RIGHT);
		levelset = Charset(status["lvNumber"], Charset::Alignment::LEFT);

		namelabel = OutlinedText(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::GALLERY, Color::Name::TUNA);

		quickslot[0] = quickSlot["backgrnd"];
		quickslot[1] = quickSlot["layer:cover"];

		load_padslots();

		Point<int16_t> buttonPos = Point<int16_t>(591 + pos_adj, 73);

		if (VWIDTH == 1024)
			buttonPos += Point<int16_t>(38, 0);
		else if (VWIDTH == 1280)
			buttonPos += Point<int16_t>(31, 0);
		else if (VWIDTH == 1366)
			buttonPos += Point<int16_t>(33, 0);
		else if (VWIDTH == 1920)
			buttonPos += Point<int16_t>(310, 0);

		// THE MENU ROW, MINUS EVENT AND COMMUNITY.
		//
		// All six are 34x37 and every one is created at the SAME buttonPos -
		// what spreads them out is an origin baked into each piece of artwork,
		// one per 35 pixels:
		//
		//   CashShop 590   Event 625   Character 660
		//   Community 695  Setting 730  Menu 765
		//
		// Event opens a schedule this server does not run and Community opens
		// friend and guild lists nobody here uses, so they are gone. The four
		// that remain shift right to close the gap, which frees 590 and 625 for
		// SHOUT and SPEAK - the two things actually used every session.
		//
		// The shift is applied to buttonPos per button rather than by editing
		// origins, because that artwork is shared with the submenu code, which
		// still positions itself from them.
		// wanted_left for each, walking leftwards from the screen edge, then
		// the origin cancelled back out - see MENU_RIGHT in the header.
		auto place = [&](const char* art, int16_t slot) -> Point<int16_t>
			{
				nl::node node = menu[art]["normal"]["0"];

				int16_t wanted = static_cast<int16_t>(
					MENU_RIGHT - MENU_PITCH * (slot + 1));

				int16_t ox = node ? Texture(node).get_origin().x() : 0;

				return Point<int16_t>(
					static_cast<int16_t>(wanted + ox * MENU_SCALE),
					buttonPos.y());
			};

		buttons[Buttons::BT_MENU] = std::make_unique<MapleButton>(menu["button:Menu"], place("button:Menu", 0));
		buttons[Buttons::BT_OPTIONS] = std::make_unique<MapleButton>(menu["button:Setting"], place("button:Setting", 1));
		buttons[Buttons::BT_CHARACTER] = std::make_unique<MapleButton>(menu["button:Character"], place("button:Character", 2));
		buttons[Buttons::BT_CASHSHOP] = std::make_unique<MapleButton>(menu["button:CashShop"], place("button:CashShop", 3));
		buttons[Buttons::BT_COMMUNITY] = std::make_unique<MapleButton>(menu["button:Community"], buttonPos);
		buttons[Buttons::BT_EVENT] = std::make_unique<MapleButton>(menu["button:Event"], buttonPos);

		// BIGGER, AND SPREAD TO MATCH.
		//
		// 34x37 is a 2005 mouse target. These are pressed with a thumb on a
		// handheld, and the artwork is a bitmap so growing it is the only lever
		// there is.
		//
		// The row has to be re-spaced as well: the buttons are 35 apart by the
		// origins baked into their artwork, so scaling alone would overlap them
		// and the leftmost would swallow its neighbour's presses. Each one is
		// pushed out from the right-hand end by the extra width the scale adds.
		//
		// Two of the six were retired, which is what makes the room for this.
		for (uint16_t id : { Buttons::BT_MENU, Buttons::BT_OPTIONS,
			Buttons::BT_CHARACTER, Buttons::BT_CASHSHOP })
			buttons[id]->set_scale(MENU_SCALE);

		// Built, then switched off - both are still referenced by the submenu
		// and quickslot code, and removing them outright would mean unpicking
		// all of that for two buttons nobody presses.
		buttons[Buttons::BT_COMMUNITY]->set_active(false);
		buttons[Buttons::BT_EVENT]->set_active(false);

		// SHOUT and SPEAK, on the main screen where they can always be reached.
		//
		// In-game artwork rather than anything drawn for this: the megaphone is
		// the actual Megaphone item's inventory icon, so the button looks like the
		// thing it replaces. There is no microphone anywhere in the data - nothing
		// in 2005 needed one - so SPEAK borrows the chat balloon, which is at
		// least the right idea.
		shout_icon = nl::nx::item["Cash"]["0507.img"]["05071000"]["info"]["icon"];
		speak_icon = nl::nx::ui["StatusBar3.img"]["chat"]["common"]["BtChat"]["normal"]["0"];

		// PARTY borrows the retired Community button's artwork, which is a
		// group of people - exactly the right picture, and free now that the
		// button itself is gone.
		party_icon = menu["button:Community"]["normal"]["0"];

		// Invisible hit areas over hand-drawn icons. MapleButton wants a node with
		// normal/pressed/mouseOver/disabled under it, and an item icon is a single
		// bitmap - so the button is the box and the picture is drawn separately.
		// On the menu row, in the two slots Event and Community vacated, and
		// the same 35 pixels apart as everything else on it.
		// Slots 5 and 4, continuing the same walk. These are drawn by hand
		// rather than by MapleButton, so there is no origin to cancel.
		shout_pos = Point<int16_t>(MENU_RIGHT - MENU_PITCH * 6, buttonPos.y());
		speak_pos = Point<int16_t>(MENU_RIGHT - MENU_PITCH * 5, buttonPos.y());
		party_pos = Point<int16_t>(MENU_RIGHT - MENU_PITCH * 7, buttonPos.y());

		buttons[Buttons::BT_SHOUT] = std::make_unique<AreaButton>(
			shout_pos, Point<int16_t>(EXTRA_ICON_SIZE, EXTRA_ICON_SIZE));
		buttons[Buttons::BT_SPEAK] = std::make_unique<AreaButton>(
			speak_pos, Point<int16_t>(EXTRA_ICON_SIZE, EXTRA_ICON_SIZE));
		buttons[Buttons::BT_PARTY] = std::make_unique<AreaButton>(
			party_pos, Point<int16_t>(EXTRA_ICON_SIZE, EXTRA_ICON_SIZE));

		// Nothing to hear with, nothing to press. Asked of the recogniser rather
		// than assumed, so a device with no model deployed simply does not show
		// the button instead of showing one that cannot work.
		buttons[Buttons::BT_SPEAK]->set_active(Speech::get().available());

		printf("[*] statusbar extras: shout icon %s, speak icon %s, speech %s\n",
			shout_icon.is_valid() ? "ok" : "MISSING",
			speak_icon.is_valid() ? "ok" : "MISSING",
			Speech::get().available() ? "available" : "unavailable");

		// WHERE OUR ONE MENU BUTTON GOES, at slot 0 where the game's own Menu
		// button was - so on a one-screen device the thumb finds the menu
		// where it already expected it. Worked out here because this is where
		// the row's height is known.
		panel_menu_pos = Point<int16_t>(
			static_cast<int16_t>(MENU_RIGHT - PANEL_MENU_SIZE), buttonPos.y());

		// The device's own say on the whole row. See apply_menu_policy - on
		// Android this is what empties the top of the screen.
		apply_menu_policy();

		if (quickslot_active && VWIDTH > 800)
		{
			buttons[Buttons::BT_CASHSHOP]->set_active(false);
			buttons[Buttons::BT_MENU]->set_active(false);
			buttons[Buttons::BT_OPTIONS]->set_active(false);
			buttons[Buttons::BT_CHARACTER]->set_active(false);
			buttons[Buttons::BT_COMMUNITY]->set_active(false);
			buttons[Buttons::BT_EVENT]->set_active(false);

			buttons[Buttons::BT_SHOUT]->set_active(false);
			buttons[Buttons::BT_SPEAK]->set_active(false);
			buttons[Buttons::BT_PARTY]->set_active(false);
		}

		std::string fold = "button:Fold";
		std::string extend = "button:Extend";

		if (VWIDTH == 800)
		{
			fold += "800";
			extend += "800";
		}

		if (VWIDTH == 1366)
			quickslot_qs_adj = Point<int16_t>(213, 0);
		else
			quickslot_qs_adj = Point<int16_t>(211, 0);

		if (VWIDTH == 800)
		{
			Point<int16_t> quickslot_qs = Point<int16_t>(579, 0);

			buttons[Buttons::BT_FOLD_QS] = std::make_unique<MapleButton>(quickSlot[fold], quickslot_qs);
			buttons[Buttons::BT_EXTEND_QS] = std::make_unique<MapleButton>(quickSlot[extend], quickslot_qs + quickslot_qs_adj);
		}
		else if (VWIDTH == 1024)
		{
			Point<int16_t> quickslot_qs = Point<int16_t>(627 + pos_adj, 37);

			buttons[Buttons::BT_FOLD_QS] = std::make_unique<MapleButton>(quickSlot[fold], quickslot_qs);
			buttons[Buttons::BT_EXTEND_QS] = std::make_unique<MapleButton>(quickSlot[extend], quickslot_qs + quickslot_qs_adj);
		}
		else if (VWIDTH == 1280)
		{
			Point<int16_t> quickslot_qs = Point<int16_t>(621 + pos_adj, 37);

			buttons[Buttons::BT_FOLD_QS] = std::make_unique<MapleButton>(quickSlot[fold], quickslot_qs);
			buttons[Buttons::BT_EXTEND_QS] = std::make_unique<MapleButton>(quickSlot[extend], quickslot_qs + quickslot_qs_adj);
		}
		else if (VWIDTH == 1366)
		{
			Point<int16_t> quickslot_qs = Point<int16_t>(623 + pos_adj, 37);

			buttons[Buttons::BT_FOLD_QS] = std::make_unique<MapleButton>(quickSlot[fold], quickslot_qs);
			buttons[Buttons::BT_EXTEND_QS] = std::make_unique<MapleButton>(quickSlot[extend], quickslot_qs + quickslot_qs_adj);
		}
		else if (VWIDTH == 1920)
		{
			Point<int16_t> quickslot_qs = Point<int16_t>(900 + pos_adj, 37);

			buttons[Buttons::BT_FOLD_QS] = std::make_unique<MapleButton>(quickSlot[fold], quickslot_qs);
			buttons[Buttons::BT_EXTEND_QS] = std::make_unique<MapleButton>(quickSlot[extend], quickslot_qs + quickslot_qs_adj);
		}

		if (quickslot_active)
			buttons[Buttons::BT_EXTEND_QS]->set_active(false);
		else
			buttons[Buttons::BT_FOLD_QS]->set_active(false);

#pragma region Menu
		menubackground[0] = submenu["backgrnd"]["0"];
		menubackground[1] = submenu["backgrnd"]["1"];
		menubackground[2] = submenu["backgrnd"]["2"];

		buttons[Buttons::BT_MENU_ACHIEVEMENT] = std::make_unique<MapleButton>(submenu["menu"]["button:achievement"], menu_pos);
		buttons[Buttons::BT_MENU_AUCTION] = std::make_unique<MapleButton>(submenu["menu"]["button:auction"], menu_pos);
		buttons[Buttons::BT_MENU_BATTLE] = std::make_unique<MapleButton>(submenu["menu"]["button:battleStats"], menu_pos);
		buttons[Buttons::BT_MENU_CLAIM] = std::make_unique<MapleButton>(submenu["menu"]["button:Claim"], menu_pos);
		buttons[Buttons::BT_MENU_FISHING] = std::make_unique<MapleButton>(submenu["menu"]["button:Fishing"], menu_pos + Point<int16_t>(3, 1));
		buttons[Buttons::BT_MENU_HELP] = std::make_unique<MapleButton>(submenu["menu"]["button:Help"], menu_pos);
		buttons[Buttons::BT_MENU_MEDAL] = std::make_unique<MapleButton>(submenu["menu"]["button:medal"], menu_pos);
		buttons[Buttons::BT_MENU_MONSTER_COLLECTION] = std::make_unique<MapleButton>(submenu["menu"]["button:monsterCollection"], menu_pos);
		buttons[Buttons::BT_MENU_MONSTER_LIFE] = std::make_unique<MapleButton>(submenu["menu"]["button:monsterLife"], menu_pos);
		buttons[Buttons::BT_MENU_QUEST] = std::make_unique<MapleButton>(submenu["menu"]["button:quest"], menu_pos);
		buttons[Buttons::BT_MENU_UNION] = std::make_unique<MapleButton>(submenu["menu"]["button:union"], menu_pos);

		// Temporary: every setting button is created at the same point and
		// relies on its own origin to sit in the right row of the menu. Quit
		// is present in the artwork but never appears, so report where each
		// one thinks it belongs.
		{
			const char* names[] = { "button:channel", "button:GameQuit",
									"button:keySetting", "button:option" };

			for (const char* n : names)
			{
				Texture t = Texture(submenu["setting"][n]["normal"]["0"]);

				printf("[*] setting %s: %dx%d org %d,%d at %d,%d\n", n,
					t.get_dimensions().x(), t.get_dimensions().y(),
					t.get_origin().x(), t.get_origin().y(),
					setting_pos.x(), setting_pos.y());
			}
		}

		buttons[Buttons::BT_SETTING_CHANNEL] = std::make_unique<MapleButton>(submenu["setting"]["button:channel"], setting_pos);
		buttons[Buttons::BT_SETTING_QUIT] = std::make_unique<MapleButton>(submenu["setting"]["button:GameQuit"], setting_pos);
		buttons[Buttons::BT_SETTING_JOYPAD] = std::make_unique<MapleButton>(submenu["setting"]["button:JoyPad"], setting_pos);
		buttons[Buttons::BT_SETTING_KEYS] = std::make_unique<MapleButton>(submenu["setting"]["button:keySetting"], setting_pos);
		buttons[Buttons::BT_SETTING_OPTION] = std::make_unique<MapleButton>(submenu["setting"]["button:option"], setting_pos);

		// The setting menu draws five slots but this UI version only supplies
		// four buttons - there is no `button:JoyPad` under submenu/setting -
		// so the last slot has always been an unexplained gap. Fill it with a
		// real quit, which the menu otherwise has no way to reach: the button
		// labelled GameQuit returns to character select rather than exiting.
		//
		// The other four place themselves through origins baked into their
		// sprites (-33, -61, -89, -117, a step of 28), so the fifth belongs at
		// -145. This one is borrowed from the login screen and has an origin
		// of (0,0), so it is positioned by hand, nudged to sit centred in a
		// row it is slightly too large for.
		buttons[Buttons::BT_SETTING_EXIT] = std::make_unique<MapleButton>(
			nl::nx::ui["Login.img"]["Common"]["BtExit"],
			setting_pos + SETTING_EXIT_OFFSET
		);

		buttons[Buttons::BT_COMMUNITY_PARTY] = std::make_unique<MapleButton>(submenu["community"]["button:bossParty"], community_pos);
		buttons[Buttons::BT_COMMUNITY_FRIENDS] = std::make_unique<MapleButton>(submenu["community"]["button:friends"], community_pos);
		buttons[Buttons::BT_COMMUNITY_GUILD] = std::make_unique<MapleButton>(submenu["community"]["button:guild"], community_pos);
		buttons[Buttons::BT_COMMUNITY_MAPLECHAT] = std::make_unique<MapleButton>(submenu["community"]["button:mapleChat"], community_pos);

		buttons[Buttons::BT_CHARACTER_INFO] = std::make_unique<MapleButton>(submenu["character"]["button:character"], character_pos);
		buttons[Buttons::BT_CHARACTER_EQUIP] = std::make_unique<MapleButton>(submenu["character"]["button:Equip"], character_pos);
		buttons[Buttons::BT_CHARACTER_ITEM] = std::make_unique<MapleButton>(submenu["character"]["button:Item"], character_pos);
		buttons[Buttons::BT_CHARACTER_SKILL] = std::make_unique<MapleButton>(submenu["character"]["button:Skill"], character_pos);
		buttons[Buttons::BT_CHARACTER_STAT] = std::make_unique<MapleButton>(submenu["character"]["button:Stat"], character_pos);

		buttons[Buttons::BT_EVENT_DAILY] = std::make_unique<MapleButton>(submenu["event"]["button:dailyGift"], event_pos);
		buttons[Buttons::BT_EVENT_SCHEDULE] = std::make_unique<MapleButton>(submenu["event"]["button:schedule"], event_pos);

		for (size_t i = Buttons::BT_MENU_QUEST; i <= Buttons::BT_EVENT_DAILY; i++)
			buttons[i]->set_active(false);

		menutitle[0] = submenu["title"]["character"];
		menutitle[1] = submenu["title"]["community"];
		menutitle[2] = submenu["title"]["event"];
		menutitle[3] = submenu["title"]["menu"];
		menutitle[4] = submenu["title"]["setting"];
#pragma endregion

		// THE BAR SITS 120 FROM THE BOTTOM. IT ALWAYS DID.
		//
		// Every one of these branches wrote the same number a different way:
		// 600-480, 768-648, 720-600, and the 1920 case spelling it out as
		// 960 + (VHEIGHT - 1080). Written as heights instead of as an offset,
		// they pinned the bar to four specific screens - so a resolution the
		// list had not anticipated put it off the bottom edge, which is what
		// stood between this client and any size other than the four.
		//
		// Only the y was ever height-dependent; the x positions and widths
		// below are genuinely per-resolution and are left alone.
		if (VWIDTH == 800)
		{
			position = Point<int16_t>(0, VHEIGHT - BAR_FROM_BOTTOM);
			position_x = 410;
			position_y = position.y();
			dimension = Point<int16_t>(VWIDTH - position_x, 140);
		}
		else if (VWIDTH == 1024)
		{
			position = Point<int16_t>(0, VHEIGHT - BAR_FROM_BOTTOM);
			position_x = 410;
			position_y = position.y() + 42;
			dimension = Point<int16_t>(VWIDTH - position_x, 75);
		}
		else if (VWIDTH == 1280)
		{
			position = Point<int16_t>(0, VHEIGHT - BAR_FROM_BOTTOM);
			position_x = 500;
			position_y = position.y() + 42;
			dimension = Point<int16_t>(VWIDTH - position_x, 75);
		}
		else if (VWIDTH == 1366)
		{
			position = Point<int16_t>(0, VHEIGHT - BAR_FROM_BOTTOM);
			position_x = 585;
			position_y = position.y() + 42;
			dimension = Point<int16_t>(VWIDTH - position_x, 75);
		}
		else if (VWIDTH == 1920)
		{
			position = Point<int16_t>(0, VHEIGHT - BAR_FROM_BOTTOM);
			position_x = 860;
			position_y = position.y() + 40;
			dimension = Point<int16_t>(VWIDTH - position_x, 80);
		}
	}

	void UIStatusbar::draw(float alpha) const
	{
		// Across the TOP of the screen, not the bottom - the phone mount
		// clamps the lower edge, and a boss bar is read constantly during a
		// fight. Drawn in screen space rather than relative to the bar, since
		// it does not belong to the bar's artwork.
		if (boss_hp_ticks > 0 && boss_gage.is_active())
		{
			int16_t vwidth = Constants::Constants::get().get_viewwidth();

			boss_gage.draw(
				Point<int16_t>(static_cast<int16_t>((vwidth - boss_gage.width()) / 2), 30),
				boss_hp_percent);
		}

		UIElement::draw_sprites(alpha);

		for (size_t i = 0; i <= Buttons::BT_EVENT; i++)
			buttons.at(i)->draw(position);

		// THE GAUGES BELONG TO WHICHEVER SCREEN IS NOT THE GAME.
		//
		// The panel draws HP up its left edge, MP up its right and EXP along
		// its foot, in a frame that is always on screen and never over
		// anything. This cluster is the same three numbers a second time,
		// sitting on top of the map - so on a handheld with a panel it is
		// covering the game to say what the other screen already says.
		//
		// The whole block goes together, name and level included: they are
		// one piece of artwork, and keeping the text without the frame it is
		// printed on would leave two figures floating over the scenery.
		if (!SecondScreen::available())
		{
			hpmp_sprites[0].draw(position, alpha);

			expbar.draw(position + exp_pos);
			hpbar.draw(position + hpmp_pos);
			mpbar.draw(position + hpmp_pos);

			hpmp_sprites[1].draw(position, alpha);
			hpmp_sprites[2].draw(position, alpha);

			int16_t level = stats.get_stat(Maplestat::Id::LEVEL);
			int16_t hp = stats.get_stat(Maplestat::Id::HP);
			int16_t mp = stats.get_stat(Maplestat::Id::MP);
			int32_t maxhp = stats.get_total(Equipstat::Id::HP);
			int32_t maxmp = stats.get_total(Equipstat::Id::MP);
			int64_t exp = stats.get_exp();

			std::string expstring = std::to_string(100 * getexppercent());

			statset.draw(
				std::to_string(exp) + "[" + expstring.substr(0, expstring.find('.') + 3) + "%]",
				position + statset_pos
			);

			hpmpset.draw(
				"[" + std::to_string(hp) + "/" + std::to_string(maxhp) + "]",
				position + hpset_pos
			);

			hpmpset.draw(
				"[" + std::to_string(mp) + "/" + std::to_string(maxmp) + "]",
				position + mpset_pos
			);

			levelset.draw(
				std::to_string(level),
				position + levelset_pos
			);

			namelabel.draw(position + namelabel_pos);
		}   // !SecondScreen::available() - the gauges

		// LAST of the bar's own artwork, so nothing drawn afterwards can
		// cover them - which is exactly what the HP/MP frame was doing.
		buttons.at(Buttons::BT_SHOUT)->draw(position);
		buttons.at(Buttons::BT_SPEAK)->draw(position);
		buttons.at(Buttons::BT_PARTY)->draw(position);

		// PLUS THE ORIGIN, or the picture and its hit box are in different
		// places.
		//
		// A Texture draws itself relative to its origin, and an item icon's is
		// not (0,0) - the megaphone's is (-2,31). Drawn at EXTRA_POS its
		// top-left landed 31 pixels ABOVE the AreaButton, so it looked like a
		// button that did nothing when clicked, when really it was a picture
		// sitting a full icon height off its own target. Adding the origin back
		// pins the top-left where it is wanted, whatever the anchor.
		//
		// draw_padslot_icon in this same file already says all this. It was
		// written after the identical mistake put the quickslot icons an icon
		// height too high.
		// Both stretched to a menu button's size, so the row reads as one row.
		// The megaphone is 29x31 and the balloon only 18x18 - authored for a
		// corner of the chat bar - and at their own sizes they looked like
		// leftovers rather than controls.
		// GATED ON THE BUTTON, like speak and party below. This one drew
		// unconditionally, so retiring the megaphone would have left its
		// picture on screen over a button that no longer answered.
		if (buttons.at(Buttons::BT_SHOUT)->is_active())
		{
			Point<int16_t> shout_at = position + shout_pos + shout_icon.get_origin();

			shout_icon.draw(DrawArgument(
				shout_at, shout_at, Point<int16_t>(EXTRA_ICON_SIZE, EXTRA_ICON_SIZE), 1.0f, 1.0f, 1.0f, 0.0f));
		}

		// The BUTTON's own state, not Speech::available() - that crosses into
		// Java, and asking it sixty times a second to draw one small picture is
		// not what it is for. It was decided once, when the bar was built.
		if (buttons.at(Buttons::BT_SPEAK)->is_active())
		{
			Point<int16_t> speak_at = position + speak_pos + speak_icon.get_origin();

			speak_icon.draw(DrawArgument(
				speak_at, speak_at, Point<int16_t>(EXTRA_ICON_SIZE, EXTRA_ICON_SIZE), 1.0f, 1.0f, 1.0f, 0.0f));
		}

		if (buttons.at(Buttons::BT_PARTY)->is_active())
		{
			Point<int16_t> party_at = position + party_pos + party_icon.get_origin();

			party_icon.draw(DrawArgument(
				party_at, party_at, Point<int16_t>(EXTRA_ICON_SIZE, EXTRA_ICON_SIZE), 1.0f, 1.0f, 1.0f, 0.0f));
		}


		draw_panel_menu();

		buttons.at(Buttons::BT_FOLD_QS)->draw(position + quickslot_adj);
		buttons.at(Buttons::BT_EXTEND_QS)->draw(position + quickslot_adj - quickslot_qs_adj);

		Point<int16_t> quickslot_bar = quickslot_bar_pos();

		quickslot[0].draw(quickslot_bar);
		quickslot[1].draw(quickslot_bar);

		// After the cover, so the labels are not drawn over by the bar's own
		// frame - they are the point of the thing.
		draw_padslots(quickslot_bar);

#pragma region Menu
		Point<int16_t> pos_adj = Point<int16_t>(0, 0);

		if (quickslot_active)
		{
			if (VWIDTH == 800)
				pos_adj += Point<int16_t>(0, -73);
			else
				pos_adj += Point<int16_t>(0, -31);
		}

		Point<int16_t> pos;
		uint8_t button_count, menutitle_index;

		if (character_active)
		{
			pos = character_pos;
			button_count = 5;
			menutitle_index = 0;
		}
		else if (community_active)
		{
			pos = community_pos;
			button_count = 4;
			menutitle_index = 1;
		}
		else if (event_active)
		{
			pos = event_pos;
			button_count = 2;
			menutitle_index = 2;
		}
		else if (menu_active)
		{
			pos = menu_pos;
			button_count = 11;
			menutitle_index = 3;
		}
		else if (setting_active)
		{
			pos = setting_pos;
			button_count = 5;
			menutitle_index = 4;
		}
		else
		{
			return;
		}

		Point<int16_t> mid_pos = Point<int16_t>(0, 29);

		uint16_t end_y = std::floor(28.2 * button_count);

		if (menu_active)
			end_y -= 1;

		uint16_t mid_y = end_y - mid_pos.y();

		menubackground[0].draw(position + pos + pos_adj);
		menubackground[1].draw(DrawArgument(position + pos + pos_adj) + DrawArgument(mid_pos, Point<int16_t>(0, mid_y)));
		menubackground[2].draw(position + pos + pos_adj + Point<int16_t>(0, end_y));

		menutitle[menutitle_index].draw(position + pos + pos_adj);

		for (size_t i = Buttons::BT_MENU_QUEST; i <= Buttons::BT_EVENT_DAILY; i++)
			buttons.at(i)->draw(position);
#pragma endregion
	}

	void UIStatusbar::update()
	{
		UIElement::update();

		// Age the boss bar out. Nothing announces the end of a boss fight, so
		// this is the only thing that ever hides it.
		if (boss_hp_ticks > 0 && --boss_hp_ticks <= 0)
			boss_gage.clear();

		for (auto sprite : hpmp_sprites)
			sprite.update();

		expbar.update(getexppercent());
		hpbar.update(gethppercent());
		mpbar.update(getmppercent());

		namelabel.change_text(stats.get_name());

		Point<int16_t> pos_adj = get_quickslot_pos();

		if (quickslot_active)
		{
			if (quickslot_adj.x() > quickslot_min)
			{
				int16_t new_x = quickslot_adj.x() - Constants::TIMESTEP;

				if (new_x < quickslot_min)
					quickslot_adj.set_x(quickslot_min);
				else
					quickslot_adj.shift_x(-Constants::TIMESTEP);
			}
		}
		else
		{
			if (quickslot_adj.x() < QUICKSLOT_MAX)
			{
				int16_t new_x = quickslot_adj.x() + Constants::TIMESTEP;

				if (new_x > QUICKSLOT_MAX)
					quickslot_adj.set_x(QUICKSLOT_MAX);
				else
					quickslot_adj.shift_x(Constants::TIMESTEP);
			}
		}

		for (size_t i = Buttons::BT_MENU_QUEST; i <= Buttons::BT_MENU_CLAIM; i++)
		{
			Point<int16_t> menu_adj = Point<int16_t>(0, 0);

			if (i == Buttons::BT_MENU_FISHING)
				menu_adj = Point<int16_t>(3, 1);

			buttons[i]->set_position(menu_pos + menu_adj + pos_adj);
		}

		for (size_t i = Buttons::BT_SETTING_CHANNEL; i <= Buttons::BT_SETTING_QUIT; i++)
			buttons[i]->set_position(setting_pos + pos_adj);

		// Placed by hand rather than by a sprite origin, so it is not part of
		// the loop above.
		buttons[Buttons::BT_SETTING_EXIT]->set_position(setting_pos + pos_adj + SETTING_EXIT_OFFSET);

		for (size_t i = Buttons::BT_COMMUNITY_FRIENDS; i <= Buttons::BT_COMMUNITY_MAPLECHAT; i++)
			buttons[i]->set_position(community_pos + pos_adj);

		for (size_t i = Buttons::BT_CHARACTER_INFO; i <= Buttons::BT_CHARACTER_ITEM; i++)
			buttons[i]->set_position(character_pos + pos_adj);

		for (size_t i = Buttons::BT_EVENT_SCHEDULE; i <= Buttons::BT_EVENT_DAILY; i++)
			buttons[i]->set_position(event_pos + pos_adj);
	}

	void UIStatusbar::update_boss_hp(const std::string& name, int8_t percent,
		int8_t color, int8_t bgcolor)
	{
		if (percent <= 0)
		{
			boss_hp_ticks = 0;
			boss_gage.clear();

			return;
		}

		boss_gage.set_mob(name, color, bgcolor);
		boss_hp_percent = static_cast<float>(percent) / 100.0f;
		boss_hp_ticks = BOSS_HP_LINGER;
	}

	Button::State UIStatusbar::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case Buttons::BT_SHOUT:
			UI::get().emplace<UIMegaphone>();
			return Button::State::NORMAL;
		case Buttons::BT_SPEAK:
			// Straight into the chat bar, which is already the thing that
			// talks to everyone standing on this map. The chat bar collects
			// the finished sentence itself - see its update().
			if (auto chatbar = UI::get().get_element<UIChatbar>())
				chatbar->start_dictation();
			return Button::State::NORMAL;
		case Buttons::BT_PARTY:
			// One button, two jobs, because they are never both available:
			// with no party the only useful action is to start one, and with a
			// party the only useful action is to fill or leave it.
			if (!Stage::get().get_player().get_party().is_in_party())
			{
				CreatePartyPacket().dispatch();
			}
			else if (auto hud = UI::get().get_element<UIPartyHUD>())
			{
				hud->toggle_invites();
			}

			return Button::State::NORMAL;
		case Buttons::BT_CASHSHOP:
			// DISABLED until SET_CASH_SHOP parses cleanly.
			//
			// Asking to enter is not a request the server can refuse politely:
			// it removes the character from the map and marks the shop open
			// before sending anything back. If the client then fails to change
			// screens - which it does, the handler throws part-way through the
			// reply - the player is left looking at a world they are no longer
			// standing in, unable to walk or use a portal, with relogging the
			// only way out.
			//
			// Re-enable once the reply is read to the end.
			OutPacket(OutPacket::Opcode::ENTER_CASHSHOP).dispatch();
			break;
		// THE BOTTOM ROW OPENS OUR PANEL, NOT THEIR ICON POPUPS.
		//
		// These five buttons are the game's own menus: a grid of small icons
		// built for a mouse on a monitor, several of which lead to things that
		// do not exist in this version at all (Union, Auction, Monster Life).
		// On a handheld with one screen they were the ONLY way to reach
		// anything, and they were the wrong way.
		//
		// Where there is a real second screen open_overlay answers false and
		// every one of these falls through to exactly what it always did, so
		// the Thor is untouched.
		case Buttons::BT_MENU:
			// HOME, not a section: this one icon replaces the entire row, so
			// it opens the top of the menu rather than any one branch of it.
			if (!SecondScreen::open_overlay(SecondScreen::Section::HOME))
				toggle_menu();

			break;
		case Buttons::BT_OPTIONS:
			if (!SecondScreen::open_overlay(SecondScreen::Section::SETTINGS))
				toggle_setting();

			break;
		case Buttons::BT_CHARACTER:
			if (!SecondScreen::open_overlay(SecondScreen::Section::CHARACTER))
				toggle_character();

			break;
		case Buttons::BT_COMMUNITY:
			if (!SecondScreen::open_overlay(SecondScreen::Section::SOCIAL))
				toggle_community();

			break;
		case Buttons::BT_EVENT:
			if (!SecondScreen::open_overlay(SecondScreen::Section::DAILY))
				toggle_event();

			break;
		case Buttons::BT_FOLD_QS:
			toggle_qs(false);
			break;
		case Buttons::BT_EXTEND_QS:
			toggle_qs(true);
			break;
		case Buttons::BT_MENU_QUEST:
			UI::get().emplace<UIQuestLog>(
				Stage::get().get_player().get_quests()
				);

			remove_menus();
			break;
		case Buttons::BT_MENU_MEDAL:
		case Buttons::BT_MENU_UNION:
		case Buttons::BT_MENU_MONSTER_COLLECTION:
		case Buttons::BT_MENU_AUCTION:
		case Buttons::BT_MENU_MONSTER_LIFE:
		case Buttons::BT_MENU_BATTLE:
		case Buttons::BT_MENU_ACHIEVEMENT:
		case Buttons::BT_MENU_FISHING:
		case Buttons::BT_MENU_HELP:
		case Buttons::BT_MENU_CLAIM:
			remove_menus();
			break;
		case Buttons::BT_SETTING_CHANNEL:
			UI::get().emplace<UIChannel>();

			remove_menus();
			break;
		case Buttons::BT_SETTING_OPTION:
			UI::get().emplace<UIOptionMenu>();

			remove_menus();
			break;
		case Buttons::BT_SETTING_KEYS:
			UI::get().emplace<UIKeyConfig>(
				Stage::get().get_player().get_inventory(),
				Stage::get().get_player().get_skills()
				);

			remove_menus();
			break;
		case Buttons::BT_SETTING_JOYPAD:
			UI::get().emplace<UIJoypad>();

			remove_menus();
			break;
		case Buttons::BT_SETTING_QUIT:
			// The rich version needs artwork our UI data does not have. Left
			// unguarded it drew nothing but a dark screen, took focus, and
			// trapped the player with no way back in and no way out - there
			// is no Escape key on a handheld.
			if (UIQuit::has_artwork())
				UI::get().emplace<UIQuit>(stats);
			else
				UI::get().emplace<UIYesNo>(
					"Do you want to return to the character select screen?",
					[](bool yes)
					{
						if (yes)
							UIQuit::return_to_charselect();
					}
				);

			remove_menus();
			break;
		case Buttons::BT_SETTING_EXIT:
			// Leaves the game entirely - the same confirmation the device's
			// own back gesture raises, so both routes out behave alike.
			UI::get().emplace<UIQuitConfirm>();

			remove_menus();
			break;
		case Buttons::BT_COMMUNITY_FRIENDS:
		case Buttons::BT_COMMUNITY_PARTY:
		{
			auto userlist = UI::get().get_element<UIUserList>();
			auto tab = (id == Buttons::BT_COMMUNITY_FRIENDS) ? UIUserList::Tab::FRIEND : UIUserList::Tab::PARTY;

			if (!userlist)
			{
				UI::get().emplace<UIUserList>(tab);
			}
			else
			{
				auto cur_tab = userlist->get_tab();
				auto is_active = userlist->is_active();

				if (cur_tab == tab)
				{
					if (is_active)
						userlist->deactivate();
					else
						userlist->makeactive();
				}
				else
				{
					if (!is_active)
						userlist->makeactive();

					userlist->change_tab(tab);
				}
			}

			remove_menus();
		}
		break;
		case Buttons::BT_COMMUNITY_GUILD:
			remove_menus();
			break;
		case Buttons::BT_COMMUNITY_MAPLECHAT:
			UI::get().emplace<UIChat>();

			remove_menus();
			break;
		case Buttons::BT_CHARACTER_INFO:
			remove_menus();
			break;
		case Buttons::BT_CHARACTER_STAT:
			UI::get().emplace<UIStatsinfo>(
				Stage::get().get_player().get_stats()
				);

			remove_menus();
			break;
		case Buttons::BT_CHARACTER_SKILL:
			UI::get().emplace<UISkillbook>(
				Stage::get().get_player().get_stats(),
				Stage::get().get_player().get_skills()
				);

			remove_menus();
			break;
		case Buttons::BT_CHARACTER_EQUIP:
			UI::get().emplace<UIEquipInventory>(
				Stage::get().get_player().get_inventory()
				);

			remove_menus();
			break;
		case Buttons::BT_CHARACTER_ITEM:
			UI::get().emplace<UIItemInventory>(
				Stage::get().get_player().get_inventory()
				);

			remove_menus();
			break;
		case Buttons::BT_EVENT_SCHEDULE:
			UI::get().emplace<UIEvent>();

			remove_menus();
			break;
		case Buttons::BT_EVENT_DAILY:
			remove_menus();
			break;
		}

		return Button::State::NORMAL;
	}

	void UIStatusbar::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				if (!menu_active && !setting_active && !community_active && !character_active && !event_active)
					toggle_setting();
				else
					remove_menus();
			}
			else if (keycode == KeyAction::Id::RETURN)
			{
				for (size_t i = Buttons::BT_MENU_QUEST; i <= Buttons::BT_EVENT_DAILY; i++)
					if (buttons[i]->get_state() == Button::State::MOUSEOVER)
						button_pressed(i);
			}
			else if (keycode == KeyAction::Id::UP || keycode == KeyAction::Id::DOWN)
			{
				uint16_t min_id, max_id;

				if (menu_active)
				{
					min_id = Buttons::BT_MENU_QUEST;
					max_id = Buttons::BT_MENU_CLAIM;
				}
				else if (setting_active)
				{
					min_id = Buttons::BT_SETTING_CHANNEL;
					max_id = Buttons::BT_SETTING_EXIT;
				}
				else if (community_active)
				{
					min_id = Buttons::BT_COMMUNITY_FRIENDS;
					max_id = Buttons::BT_COMMUNITY_MAPLECHAT;
				}
				else if (character_active)
				{
					min_id = Buttons::BT_CHARACTER_INFO;
					max_id = Buttons::BT_CHARACTER_ITEM;
				}
				else if (event_active)
				{
					min_id = Buttons::BT_EVENT_SCHEDULE;
					max_id = Buttons::BT_EVENT_DAILY;
				}

				uint16_t id = min_id;

				for (size_t i = min_id; i <= max_id; i++)
				{
					if (buttons[i]->get_state() != Button::State::NORMAL)
					{
						id = i;

						buttons[i]->set_state(Button::State::NORMAL);
						break;
					}
				}

				if (keycode == KeyAction::Id::DOWN)
				{
					if (id < max_id)
						id++;
					else
						id = min_id;
				}
				else if (keycode == KeyAction::Id::UP)
				{
					if (id > min_id)
						id--;
					else
						id = max_id;
				}

				buttons[id]->set_state(Button::State::MOUSEOVER);
			}
		}
	}

	bool UIStatusbar::is_in_range(Point<int16_t> cursorpos) const
	{
		Point<int16_t> pos;
		Rectangle<int16_t> bounds;

		if (!character_active && !community_active && !event_active && !menu_active && !setting_active)
		{
			pos = Point<int16_t>(position_x, position_y);
			bounds = Rectangle<int16_t>(pos, pos + dimension);
		}
		else
		{
			uint8_t button_count;
			int16_t pos_y_adj;

			if (character_active)
			{
				pos = character_pos;
				button_count = 5;
				pos_y_adj = 248;
			}
			else if (community_active)
			{
				pos = community_pos;
				button_count = 4;
				pos_y_adj = 301;
			}
			else if (event_active)
			{
				pos = event_pos;
				button_count = 2;
				pos_y_adj = 417;
			}
			else if (menu_active)
			{
				pos = menu_pos;
				button_count = 11;
				pos_y_adj = -90;
			}
			else if (setting_active)
			{
				pos = setting_pos;
				button_count = 5;
				pos_y_adj = 248;
			}

			pos_y_adj += VHEIGHT - 600;

			Point<int16_t> pos_adj = const_cast<UIStatusbar*>(this)->get_quickslot_pos();
			pos = Point<int16_t>(pos.x(), std::abs(pos.y()) + pos_y_adj) + pos_adj;

			uint16_t end_y = std::floor(28.2 * button_count);

			bounds = Rectangle<int16_t>(pos, pos + Point<int16_t>(113, end_y + 35));
		}

		return bounds.contains(cursorpos);
	}

	UIElement::Type UIStatusbar::get_type() const
	{
		return TYPE;
	}

	void UIStatusbar::toggle_qs()
	{
		if (!menu_active && !setting_active && !community_active && !character_active && !event_active)
			toggle_qs(!quickslot_active);
	}

	void UIStatusbar::toggle_qs(bool quick_slot_active)
	{
		if (quickslot_active == quick_slot_active)
			return;

		quickslot_active = quick_slot_active;
		buttons[Buttons::BT_FOLD_QS]->set_active(quickslot_active);
		buttons[Buttons::BT_EXTEND_QS]->set_active(!quickslot_active);

		if (VWIDTH > 800)
		{
			buttons[Buttons::BT_CASHSHOP]->set_active(!quickslot_active);
			buttons[Buttons::BT_MENU]->set_active(!quickslot_active);
			buttons[Buttons::BT_OPTIONS]->set_active(!quickslot_active);
			buttons[Buttons::BT_CHARACTER]->set_active(!quickslot_active);
			// Deliberately NOT restored - see the constructor. These two are
			// retired, and the quickslot toggle was what quietly put them back
			// on screen.
			buttons[Buttons::BT_COMMUNITY]->set_active(false);
			buttons[Buttons::BT_EVENT]->set_active(false);

			// SHOUT and SPEAK hide with the rest of the row.
			//
			// The extended quickslot bar covers the ground these two stand on,
			// and UIStatusbar::send_cursor tests them BEFORE anything else - so
			// left visible they swallowed taps meant for the bar underneath,
			// the fold button among them. That is why the quickslot would come
			// out and refuse to go back in.
			buttons[Buttons::BT_SHOUT]->set_active(!quickslot_active);
			buttons[Buttons::BT_SPEAK]->set_active(
				!quickslot_active && Speech::get().available());
			buttons[Buttons::BT_PARTY]->set_active(!quickslot_active);
		}

		// AND THEN THE DEVICE HAS THE LAST WORD. Folding the quickslot bar
		// used to put the whole retired row back on screen - the same way it
		// once resurrected Community and Event.
		apply_menu_policy();
	}

	// THE TOP SCREEN'S MENU ROW BELONGS TO THE PANEL NOW.
	//
	// Seven controls sat across the top of the game: the game's own Menu,
	// Settings, Character and Cash Shop popups, plus a megaphone, a
	// voice-to-text button and a party button. Every one of them is in the
	// panel's menu, most of them one press in - and all of them were designed
	// for a mouse on a monitor, then scaled up here to be thumbable, which is
	// why they take up so much of a handheld's screen.
	//
	//   Thor  - nothing at all. The panel is on the other screen permanently,
	//           so a second way in is just clutter over the map.
	//   RP5   - ONE button. The row is what was asked to be REPLACED, not
	//           supplemented, and one target labelled Menu is easier to hit
	//           than four unlabelled ones.
	//
	// Cash Shop is not lost with the rest: it moved onto the panel's Home
	// menu, which is the only way in once this row is gone.
	void UIStatusbar::draw_panel_menu() const
	{
		if (!SecondScreen::overlay_supported())
			return;

		if (!buttons.at(Buttons::BT_MENU)->is_active())
			return;

		if (!panel_menu_tried)
		{
			panel_menu_tried = true;

			// The panel's own Home artwork, so the way in looks like where it
			// goes. IconAlert is the same badge the panel puts on its own
			// buttons - one picture, so a mark means the same thing wherever
			// it appears.
			panel_menu_icon = nl::nx::map001["Custom"]["IconHome"];
			panel_menu_badge = nl::nx::map001["Custom"]["IconAlert"];

			panel_menu_label = Text(Text::Font::A11M, Text::Alignment::CENTER,
				Color::Name::WHITE);
		}

		Point<int16_t> at = position + panel_menu_pos;

		if (panel_menu_icon.is_valid())
		{
			// The word takes the bottom of the box, so the picture gets the
			// rest - it is a labelled button, not an icon with a caption
			// floating under it.
			constexpr int16_t WORD = 14;
			int16_t art = static_cast<int16_t>(PANEL_MENU_SIZE - WORD);

			Point<int16_t> art_at = at + panel_menu_icon.get_origin();

			panel_menu_icon.draw(DrawArgument(
				art_at, art_at, Point<int16_t>(art, art), 1.0f, 1.0f, 1.0f, 0.0f));

			panel_menu_label.change_text("MENU");
			panel_menu_label.draw(Point<int16_t>(
				static_cast<int16_t>(at.x() + PANEL_MENU_SIZE / 2),
				static_cast<int16_t>(at.y() + art - 2)));

			// THE BADGE THE THOR ALREADY HAD. With the row collapsed to one
			// icon there is nowhere else for it to go, so this one carries
			// everything inside: points to spend, post waiting, a daily left
			// unclaimed.
			if (panel_menu_badge.is_valid() && SecondScreen::overlay_alert())
			{
				constexpr int16_t BADGE = 16;

				Point<int16_t> badge_at = Point<int16_t>(
					static_cast<int16_t>(at.x() + PANEL_MENU_SIZE - BADGE),
					at.y());

				panel_menu_badge.draw(DrawArgument(
					badge_at + panel_menu_badge.get_origin(),
					badge_at + panel_menu_badge.get_origin(),
					Point<int16_t>(BADGE, BADGE), 1.0f, 1.0f, 1.0f, 0.0f));
			}
		}
	}

	void UIStatusbar::apply_menu_policy()
	{
		// The one button that survives on a one-screen device, and only
		// there - the Thor reaches all of this downstairs.
		bool keep_menu = SecondScreen::overlay_supported();

		if (keep_menu)
		{
			// OUR BUTTON, NOT THEIRS. A MapleButton draws its own artwork, so
			// leaving the game's Menu sprite in place and putting our icon on
			// top would show both. An AreaButton is a hit box with no picture
			// - the same trick the megaphone and party buttons already use -
			// and draw_panel_menu supplies the picture.
			//
			// panel_menu_pos is set in the constructor, where the row's y is
			// worked out - this runs again whenever the quickslot bar folds,
			// long after that is out of scope.
			buttons[Buttons::BT_MENU] = std::make_unique<AreaButton>(
				panel_menu_pos,
				Point<int16_t>(PANEL_MENU_SIZE, PANEL_MENU_SIZE));
		}

		buttons[Buttons::BT_MENU]->set_active(keep_menu);

		for (uint16_t id : { Buttons::BT_CASHSHOP, Buttons::BT_OPTIONS,
			Buttons::BT_CHARACTER, Buttons::BT_COMMUNITY, Buttons::BT_EVENT,
			Buttons::BT_SHOUT, Buttons::BT_SPEAK, Buttons::BT_PARTY })
			buttons[id]->set_active(false);
	}

	void UIStatusbar::toggle_menu()
	{
		remove_active_menu(MenuType::MENU);

		menu_active = !menu_active;

		buttons[Buttons::BT_MENU_ACHIEVEMENT]->set_active(menu_active);
		buttons[Buttons::BT_MENU_AUCTION]->set_active(menu_active);
		buttons[Buttons::BT_MENU_BATTLE]->set_active(menu_active);
		buttons[Buttons::BT_MENU_CLAIM]->set_active(menu_active);
		buttons[Buttons::BT_MENU_FISHING]->set_active(menu_active);
		buttons[Buttons::BT_MENU_HELP]->set_active(menu_active);
		buttons[Buttons::BT_MENU_MEDAL]->set_active(menu_active);
		buttons[Buttons::BT_MENU_MONSTER_COLLECTION]->set_active(menu_active);
		buttons[Buttons::BT_MENU_MONSTER_LIFE]->set_active(menu_active);
		buttons[Buttons::BT_MENU_QUEST]->set_active(menu_active);
		buttons[Buttons::BT_MENU_UNION]->set_active(menu_active);

		if (menu_active)
		{
			buttons[Buttons::BT_MENU_QUEST]->set_state(Button::State::MOUSEOVER);

			Sound(Sound::Name::DLGNOTICE).play();
		}
	}

	void UIStatusbar::toggle_setting()
	{
		remove_active_menu(MenuType::SETTING);

		setting_active = !setting_active;

		buttons[Buttons::BT_SETTING_CHANNEL]->set_active(setting_active);
		buttons[Buttons::BT_SETTING_QUIT]->set_active(setting_active);
		buttons[Buttons::BT_SETTING_JOYPAD]->set_active(setting_active);
		buttons[Buttons::BT_SETTING_KEYS]->set_active(setting_active);
		buttons[Buttons::BT_SETTING_OPTION]->set_active(setting_active);
		buttons[Buttons::BT_SETTING_EXIT]->set_active(setting_active);

		if (setting_active)
		{
			buttons[Buttons::BT_SETTING_CHANNEL]->set_state(Button::State::MOUSEOVER);

			Sound(Sound::Name::DLGNOTICE).play();
		}
	}

	void UIStatusbar::toggle_community()
	{
		remove_active_menu(MenuType::COMMUNITY);

		community_active = !community_active;

		buttons[Buttons::BT_COMMUNITY_PARTY]->set_active(community_active);
		buttons[Buttons::BT_COMMUNITY_FRIENDS]->set_active(community_active);
		buttons[Buttons::BT_COMMUNITY_GUILD]->set_active(community_active);
		buttons[Buttons::BT_COMMUNITY_MAPLECHAT]->set_active(community_active);

		if (community_active)
		{
			buttons[Buttons::BT_COMMUNITY_FRIENDS]->set_state(Button::State::MOUSEOVER);

			Sound(Sound::Name::DLGNOTICE).play();
		}
	}

	void UIStatusbar::toggle_character()
	{
		remove_active_menu(MenuType::CHARACTER);

		character_active = !character_active;

		buttons[Buttons::BT_CHARACTER_INFO]->set_active(character_active);
		buttons[Buttons::BT_CHARACTER_EQUIP]->set_active(character_active);
		buttons[Buttons::BT_CHARACTER_ITEM]->set_active(character_active);
		buttons[Buttons::BT_CHARACTER_SKILL]->set_active(character_active);
		buttons[Buttons::BT_CHARACTER_STAT]->set_active(character_active);

		if (character_active)
		{
			buttons[Buttons::BT_CHARACTER_INFO]->set_state(Button::State::MOUSEOVER);

			Sound(Sound::Name::DLGNOTICE).play();
		}
	}

	void UIStatusbar::toggle_event()
	{
		remove_active_menu(MenuType::EVENT);

		event_active = !event_active;

		buttons[Buttons::BT_EVENT_DAILY]->set_active(event_active);
		buttons[Buttons::BT_EVENT_SCHEDULE]->set_active(event_active);

		if (event_active)
		{
			buttons[Buttons::BT_EVENT_SCHEDULE]->set_state(Button::State::MOUSEOVER);

			Sound(Sound::Name::DLGNOTICE).play();
		}
	}

	void UIStatusbar::remove_menus()
	{
		if (menu_active)
			toggle_menu();
		else if (setting_active)
			toggle_setting();
		else if (community_active)
			toggle_community();
		else if (character_active)
			toggle_character();
		else if (event_active)
			toggle_event();
	}

	void UIStatusbar::remove_active_menu(MenuType type)
	{
		for (size_t i = Buttons::BT_MENU_QUEST; i <= Buttons::BT_EVENT_DAILY; i++)
			buttons[i]->set_state(Button::State::NORMAL);

		if (menu_active && type != MenuType::MENU)
			toggle_menu();
		else if (setting_active && type != MenuType::SETTING)
			toggle_setting();
		else if (community_active && type != MenuType::COMMUNITY)
			toggle_community();
		else if (character_active && type != MenuType::CHARACTER)
			toggle_character();
		else if (event_active && type != MenuType::EVENT)
			toggle_event();
	}

	Point<int16_t> UIStatusbar::get_quickslot_pos()
	{
		if (quickslot_active)
		{
			if (VWIDTH == 800)
				return Point<int16_t>(0, -73);
			else
				return Point<int16_t>(0, -31);
		}

		return Point<int16_t>(0, 0);
	}

	int16_t UIStatusbar::padslot_keycode(size_t slot) const
	{
		return slot < QUICKSLOT_COUNT ? padslots[slot].keycode : -1;
	}

	std::string UIStatusbar::padslot_name(size_t slot) const
	{
		// OutlinedText is five Texts stacked; the middle one carries the word.
		return slot < QUICKSLOT_COUNT
			? padslots[slot].label.inner.get_text() : std::string();
	}

	bool UIStatusbar::bind_padslot_public(size_t slot, Keyboard::Mapping mapping)
	{
		return slot < QUICKSLOT_COUNT
			&& bind_padslot(static_cast<int16_t>(slot), mapping);
	}

	void UIStatusbar::load_padslots()
	{
		// Laid out as the buttons sit on the handheld: the top row of the bar
		// is the top row of the pad. Select has no key of its own - it is the
		// quit button, intercepted before the pad map is read - so its cell is
		// labelled but never shows a binding.
		struct Entry { const char* label; int16_t key; };

		const Entry entries[QUICKSLOT_COUNT] = {
			{ "Y",  Setting<Joystick_Y>::get().load() },
			{ "X",  Setting<Joystick_X>::get().load() },
			{ "L2", Setting<Joystick_LT>::get().load() },
			{ "R2", Setting<Joystick_RT>::get().load() },
			{ "ST", Setting<Joystick_START>::get().load() },
			{ "SE", Setting<Joystick_SELECT>::get().load() },
			{ "B",  Setting<Joystick_B>::get().load() },
			{ "A",  Setting<Joystick_A>::get().load() },
			{ "L1", Setting<Joystick_LB>::get().load() },
			{ "R1", Setting<Joystick_RB>::get().load() },
			{ "L3", Setting<Joystick_L3>::get().load() },
			{ "R3", Setting<Joystick_R3>::get().load() },
		};

		for (size_t i = 0; i < QUICKSLOT_COUNT; i++)
		{
			padslots[i].keycode = entries[i].key;
			padslots[i].label = OutlinedText(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, Color::Name::TUNA);
			padslots[i].label.change_text(entries[i].label);
		}
	}

	// Measured off the running client rather than guessed: the bar's texture
	// puts its first cell at +12,+2 from where it is drawn, and the cells step
	// 35 in both directions. A pitch one pixel out is invisible in the first
	// cell and five pixels out by the last, which is exactly how it looked.
	namespace
	{
		constexpr int16_t CELL_X = 35;
		constexpr int16_t CELL_Y = 35;
		constexpr int16_t CELL_ORIGIN_X = 12;
		constexpr int16_t CELL_ORIGIN_Y = 2;

		// Inset by a pixel so the artwork sits inside the cell's raised border.
		constexpr int16_t ICON_SIZE = 30;
		constexpr int16_t ICON_INSET = 1;

		// Faded, because the button letter is drawn over it and both have to
		// stay readable.
		constexpr float ICON_OPACITY = 0.55f;
	}

	Point<int16_t> UIStatusbar::quickslot_bar_pos() const
	{
		Point<int16_t> bar = position + quickslot_pos + quickslot_adj;

		if (VWIDTH > 800 && VWIDTH < 1366)
			bar += Point<int16_t>(-1, 0);

		return bar;
	}

	int16_t UIStatusbar::padslot_by_position(Point<int16_t> cursorpos) const
	{
		// Shut, the cells are slid off the side of the screen; a drop there is
		// aimed at whatever is underneath, not at a cell.
		if (!quickslot_active)
			return -1;

		Point<int16_t> first = quickslot_bar_pos() + Point<int16_t>(CELL_ORIGIN_X, CELL_ORIGIN_Y);
		Point<int16_t> offset = cursorpos - first;

		if (offset.x() < 0 || offset.y() < 0)
			return -1;

		int16_t col = offset.x() / CELL_X;
		int16_t row = offset.y() / CELL_Y;

		if (col >= static_cast<int16_t>(QUICKSLOT_COLS) || row >= static_cast<int16_t>(QUICKSLOT_ROWS))
			return -1;

		// The few pixels of artwork between one cell and the next used to be
		// rejected outright, so that a DROP landing in the gap went to
		// whatever was underneath rather than snapping to a neighbour. That
		// was reasonable for a mouse, which lands where it is pointed.
		//
		// These are tapped with a thumb now, and a thumb is far wider than the
		// gap - rejecting it means a tap that plainly hit the bar does
		// nothing, with no way to tell why. The cells are adjacent anyway, so
		// the one the touch falls in is unambiguous.
		return row * static_cast<int16_t>(QUICKSLOT_COLS) + col;
	}

	bool UIStatusbar::bind_padslot(int16_t slot, Keyboard::Mapping mapping)
	{
		if (slot < 0)
			return false;

		int16_t keycode = padslots[slot].keycode;

		// Select has no key of its own - it is the quit button - so nothing can
		// be bound to it.
		if (keycode <= 0)
			return false;

		if (mapping.type == KeyType::Id::NONE)
			return false;

		uint8_t maplekey = Keyboard::maple_key(keycode);

		if (maplekey == 0)
			return false;

		// Tell the server first, then apply locally, which is the order the Key
		// Bindings window uses when it saves.
		std::vector<std::tuple<KeyConfig::Key, KeyType::Id, int32_t>> updated;
		updated.emplace_back(std::make_tuple(KeyConfig::actionbyid(maplekey), mapping.type, mapping.action));

		ChangeKeyMapPacket(updated).dispatch();

		UI::get().get_keyboard().assign(maplekey, mapping.type, mapping.action);

		Sound(Sound::Name::DRAGEND).play();

		return true;
	}

	bool UIStatusbar::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		bind_padslot(padslot_by_position(cursorpos), icon.get_mapping());

		return true;
	}

	Cursor::State UIStatusbar::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// An item picked out on the panel cannot be dragged up here, so a tap
		// on a cell takes whatever the panel has selected. Nothing selected
		// means this does not fire at all and the bar behaves as before.
		if (clicked)
		{
			int16_t slot = padslot_by_position(cursorpos);

			if (slot >= 0 && bind_padslot(slot, SecondScreen::selected_mapping()))
				return Cursor::State::IDLE;
		}

		// SHOUT and SPEAK are tested before everything else.
		//
		// UIElement::send_cursor takes only the FIRST button whose bounds hold
		// the cursor, walking them in id order - and these two have the highest
		// ids in the enum, because they had to go at the end to keep the draw
		// and setting-menu loops working. So anything overlapping them, like
		// the quickslot's fold button, silently won every press and the
		// megaphone did nothing at all when clicked.
		//
		// Asked here rather than renumbering the enum, which would have moved
		// every other button relative to those loops.
		{
			Rectangle<int16_t> shout = buttons[Buttons::BT_SHOUT]->bounds(position);

			printf("[*] statusbar cursor %d,%d clicked=%d - shout box %d,%d..%d,%d %s\n",
				cursorpos.x(), cursorpos.y(), clicked ? 1 : 0,
				shout.left(), shout.top(), shout.right(), shout.bottom(),
				shout.contains(cursorpos) ? "HIT" : "miss");
		}

		for (uint16_t id : { Buttons::BT_SHOUT, Buttons::BT_SPEAK, Buttons::BT_PARTY })
		{
			auto& button = buttons[id];

			if (!button || !button->is_active())
				continue;

			if (!button->bounds(position).contains(cursorpos))
				continue;

			if (!clicked)
				return Cursor::State::CANCLICK;

			button_pressed(id);

			return Cursor::State::IDLE;
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}

	void UIStatusbar::draw_padslot_icon(const Texture& icon, Point<int16_t> cell) const
	{
		// An icon is placed by its origin, and these are not all anchored the
		// same way - the action artwork carries origin (0,32), so drawing it at
		// the cell put it a full icon height too high. Adding the origin back
		// pins the top-left corner where it is wanted whatever the anchor.
		Point<int16_t> at = cell + Point<int16_t>(ICON_INSET, ICON_INSET) + icon.get_origin();

		icon.draw(DrawArgument(at, at, Point<int16_t>(ICON_SIZE, ICON_SIZE), 1.0f, 1.0f, ICON_OPACITY, 0.0f));
	}

	void UIStatusbar::draw_padslots(Point<int16_t> bar_pos) const
	{

		const Keyboard& keyboard = UI::get().get_keyboard();

		for (size_t i = 0; i < QUICKSLOT_COUNT; i++)
		{
			Point<int16_t> cell = bar_pos + Point<int16_t>(
				12 + static_cast<int16_t>(i % QUICKSLOT_COLS) * CELL_X,
				3 + static_cast<int16_t>(i / QUICKSLOT_COLS) * CELL_Y);

			// Whatever the key config has on this button's key. Reading it
			// every frame keeps the bar honest the moment a binding changes,
			// which matters because the two windows can be open together.
			if (padslots[i].keycode > 0)
			{
				Keyboard::Mapping mapping = keyboard.get_mapping(padslots[i].keycode);
				Texture icon;

				if (mapping.type == KeyType::Id::SKILL)
					icon = SkillData::get(mapping.action).get_icon(SkillData::Icon::NORMAL);
				else if (mapping.type == KeyType::Id::ITEM)
					icon = ItemData::get(mapping.action).get_icon(false);
				else if (mapping.type != KeyType::Id::NONE)
					// Everything else - attack, jump, pick up, the windows - is
					// an action, and those are most of what ends up on a pad
					// button.
					icon = UIKeyConfig::get_action_icon(static_cast<KeyAction::Id>(mapping.action));

				if (icon.is_valid())
					draw_padslot_icon(icon, cell);
			}

			// Last, and in the corner: the letter has to stay readable on top
			// of whatever artwork is behind it.
			padslots[i].label.draw(cell + Point<int16_t>(2, -1));
		}
	}

	bool UIStatusbar::is_menu_active()
	{
		return menu_active || setting_active || community_active || character_active || event_active;
	}

	float UIStatusbar::getexppercent() const
	{
		int16_t level = stats.get_stat(Maplestat::Id::LEVEL);

		if (level >= ExpTable::LEVELCAP)
			return 0.0f;

		int64_t exp = stats.get_exp();

		return static_cast<float>(
			static_cast<double>(exp) / ExpTable::values[level]
			);
	}

	float UIStatusbar::gethppercent() const
	{
		int16_t hp = stats.get_stat(Maplestat::Id::HP);
		int32_t maxhp = stats.get_total(Equipstat::Id::HP);

		return static_cast<float>(hp) / maxhp;
	}

	float UIStatusbar::getmppercent() const
	{
		int16_t mp = stats.get_stat(Maplestat::Id::MP);
		int32_t maxmp = stats.get_total(Equipstat::Id::MP);

		return static_cast<float>(mp) / maxmp;
	}
}