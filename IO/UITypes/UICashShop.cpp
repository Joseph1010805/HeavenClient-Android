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
#include "UICashShop.h"

#include "../KeyAction.h"
#include "../UI.h"
#include "../../Timer.h"
#include "../Window.h"

#include "../Components/MapleButton.h"

#include "../Constants.h"
#include "../Graphics/GraphicsGL.h"
#include "../Audio/Audio.h"
#include "../Gameplay/Stage.h"

#include "../Net/Handlers/CashShopHandlers.h"
#include "../Net/Packets/CashShopPackets.h"
#include "../Net/Packets/GameplayPackets.h"
#include "../Net/Packets/LoginPackets.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <nlnx/nx.hpp>

namespace ms
{
	const char* UICashShop::category_name(Category c)
	{
		switch (c)
		{
		case CAT_HAT:     return "HATS";
		case CAT_FACE:    return "FACE";
		case CAT_CLOTHES: return "CLOTHES";
		case CAT_WEAPON:  return "WEAPONS";
		case CAT_PET:     return "PETS";
		case CAT_OTHER:   return "OTHER";
		default:          return "ALL";
		}
	}

	UICashShop::Category UICashShop::category_of(int32_t itemid)
	{
		// The leading digits say what an item is. 1xxxxxx is equipment, and
		// the next two narrow it down; 5xxxxxx is the cash-only pile, which
		// is mostly pets.
		int32_t group = itemid / 10000;

		if (itemid >= 5000000)
			return (itemid / 100000 == 50) ? CAT_PET : CAT_OTHER;

		switch (group)
		{
		case 100: return CAT_HAT;
		case 101:
		case 102:
		case 103: return CAT_FACE;
		case 104:
		case 105:
		case 106:
		case 107:
		case 108:
		case 110: return CAT_CLOTHES;
		default: break;
		}

		// Weapons occupy 130 through 170.
		if (group >= 130 && group <= 170)
			return CAT_WEAPON;

		return CAT_OTHER;
	}

	void UICashShop::rebuild_filter()
	{
		filtered.clear();

		for (size_t i = 0; i < items.size(); i++)
			if (current_category == CAT_ALL
				|| category_of(items[i].get_itemid()) == current_category)
				filtered.push_back(i);

		list_offset = 0;
	}

	// A plain translucent plate with a heading, standing in for the frames the
	// window's own artwork used to supply.
	void UICashShop::draw_panel(Point<int16_t> at, int16_t w, int16_t h, const char* title) const
	{
		GraphicsGL::get().drawrectangle(at.x(), at.y(), w, h, 0.0f, 0.0f, 0.0f, 0.45f);
		GraphicsGL::get().drawrectangle(at.x(), at.y(), w, 20, 1.0f, 1.0f, 1.0f, 0.10f);

		panel_title.change_text(title);
		panel_title.draw(Point<int16_t>(at.x() + 8, at.y() + 2));
	}

	UICashShop::UICashShop() : preview_index(0), menu_index(1), promotion_index(0), mvp_grade(1), mvp_exp(0.07f), list_offset(0)
	{
		// Primary English cash-shop sprites live in CashShopGL.img. A few
		// elements (item search/effect/char/list) don't have CashShopGL
		// equivalents, so they fall back to CashShop.img's Korean nodes.
		nl::node CashShop = nl::nx::ui["CashShop.img"];
		nl::node CashShopGL = nl::nx::ui["CashShopGL.img"];

		nl::node Base = CashShop["Base"];
		nl::node BaseFrame = CashShopGL["BaseFrame"];
		nl::node backgrnd = BaseFrame["background"];
		nl::node Preview = Base["Preview"];
		nl::node CSItemSearch = BaseFrame["CSItemSearch"];
		nl::node CSList = CashShop["CSList"];
		nl::node MainItem = CashShopGL["MainMenu"]["MainItem"];
		nl::node Popup = CashShopGL["Popup"];

		sprites.emplace_back(backgrnd);

		// BestNew banner has Korean text baked in — omit. Leave dimensions
		// zeroed out (item_none fallback still works).
		BestNew_dim = Point<int16_t>();

		for (size_t i = 0; i < 3; i++)
		{
			preview_sprites[i] = Preview[i];
			preview_scene[i] = Texture(Preview[i]);
		}

		for (size_t i = 0; i < 3; i++)
			buttons[Buttons::BtPreview1 + i] = std::make_unique<TwoSpriteButton>(Base["Tab"]["Disable"][i], Base["Tab"]["Enable"][i], Point<int16_t>(957 + (i * 17), 46));

		buttons[Buttons::BtPreview1]->set_state(Button::State::PRESSED);

		// English header/exit buttons from CashShopGL/BaseFrame.
		// The bitmaps carry absolute 1024x768 positions via negative origin
		// vectors (e.g. BtExit origin = (-962,-10) → draws at (962,10)), so
		// pass Point(0,0) and let the origin place them on the canvas.
		buttons[Buttons::BtExit] = std::make_unique<MapleButton>(BaseFrame["BtExit"]);
		buttons[Buttons::BtHelp] = std::make_unique<MapleButton>(BaseFrame["BtHelp"]);
		buttons[Buttons::BtCoupon] = std::make_unique<MapleButton>(BaseFrame["BtCoupon"]);

		// Korean-only elements (CSTab/Tab menu banners, CSStatus/BtMileage,
		// CSPromotionBanner, CSChar avatar buttons, CSMVPBanner) are
		// deliberately not instantiated — there are no equivalents in
		// CashShopGL.img that match this layout 1:1. (CSStatus/BtWish and
		// CSChar/BtInventory are the exception: they toggle the wishlist /
		// cash inventory sub-panels and have no English counterpart, so the
		// Korean buttons are instantiated further below.)

		Player& player = Stage::get().get_player();
		std::string pname = player.get_stats().get_name();
		std::string pjob = player.get_stats().get_jobname();

		job_label = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::SUPERNOVA, pjob);
		name_label = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::WHITE, pname);

		promotion_pos = Point<int16_t>();
		mvp_pos = Point<int16_t>();

		// English search bar under BaseFrame/CSItemSearch.
		sprites.emplace_back(CSItemSearch["background"]);

		// Charge NX numeric charset — fall back to CashShop/Base/Number.
		charge_charset = Charset(Base["Number"], Charset::Alignment::RIGHT);

		item_base = CSList["Base"];
		item_line = Base["line"];
		item_none = Base["noItem"];
		// Korean CSEffect labels omitted; item_labels stays empty so no
		// HOT/NEW/SALE sticker draws with Korean text.

		// The real catalog from Etc/Commodity.img — the same data Cosmic's
		// CashItemFactory sells from, so the SNs and NX prices match the
		// server exactly. Only OnSale entries are listed.
		for (nl::node entry : nl::nx::etc["Commodity.img"])
		{
			int32_t onsale = entry["OnSale"];

			if (onsale != 1)
				continue;

			int32_t itemid = entry["ItemId"];

			if (!ItemData::get(itemid).is_valid())
				continue;

			int32_t sn = entry["SN"];
			int32_t price = entry["Price"];
			int32_t count = entry["Count"];

			items.push_back(Item(itemid, sn, Item::Label::NONE, price, static_cast<uint16_t>(count)));
		}

		panel_title = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::WHITE);
		tab_label = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE);
		wallet_label = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::SUPERNOVA);

		rebuild_filter();

		for (size_t i = 0; i < MAX_ITEMS; i++)
		{
			div_t div = std::div(i, GRID_COLS);

			// The GL BUY bitmap carries origin (-81, -54); this lands it
			// at the classic card's bottom strip (in-card 9,151)
			buttons[Buttons::BtBuy + i] = std::make_unique<MapleButton>(MainItem["BtBuy"], Point<int16_t>(GRID_X + STRIDE_X * div.rem - 72, GRID_Y + STRIDE_Y * div.quot + 97));

			item_name[i] = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::MINESHAFT);
			item_price[i] = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::DUSTYGRAY);
			item_discount[i] = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::LIGHTGREY);
			item_percent[i] = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::RED);
		}

		// Just right of the grid rather than out at 770, which was
		// beyond the items and on top of the preview.
		Point<int16_t> slider_pos = Point<int16_t>(608, GRID_Y);

		list_slider = Slider(
			Slider::Type::DEFAULT,
			Range<int16_t>(slider_pos.y(), slider_pos.y() + GRID_ROWS * STRIDE_Y - 30),
			slider_pos.x(),
			GRID_ROWS,
			static_cast<int16_t>(items.size() / GRID_COLS + 1),
			[&](bool upwards)
			{
				int16_t shift = upwards ? -GRID_COLS : GRID_COLS;
				bool above = list_offset >= 0;
				bool below = list_offset + shift < items.size();

				if (above && below)
				{
					list_offset += shift;

					update_items();
				}
			}
		);

		update_items();

		// === Sub-panel backgrounds (English — CashShopGL/Popup) ===
		// Popup/Gift/background and Popup/Cupon/background are the English
		// popup chrome (the nodes are named "background", not "backgrnd").
		// Popup/Buy has no single background bitmap (it is composed of
		// Top/Middle/Bottom strips), so purchase_bg stays empty and its
		// draw call remains guarded by is_valid().
		gift_bg = Texture(Popup["Gift"]["background"]);
		coupon_bg = Texture(Popup["Cupon"]["background"]);

		// Wishlist ("CART INVENTORY") and cash inventory ("CASH INVENTORY")
		// use the English right-side menu panels — 242px wide with a 6x2
		// grid of 32x32 item slots at (8 + 36 * col, 28 + 35 * row).
		nl::node RightMenu = CashShopGL["RightMenu"];
		wishlist_bg = Texture(RightMenu["CartInven"]["background"]);
		cs_inventory_bg = Texture(RightMenu["CashInven"]["background"]);

		// No wishlist/inventory toggle buttons: the cash inventory renders
		// permanently into its baked right-column panel, and the wishlist
		// has no English art (its data is never parsed client-side).

		// Charge NX under the baked NX-balance panel at (9,646); the
		// button bitmap's origin drops it just below the panel
		buttons[Buttons::BtChargeNX] = std::make_unique<MapleButton>(CashShopGL["LeftMenu"]["MyMenu"]["BtChageNx"], Point<int16_t>(10, 646));

		for (int i = 0; i < 3; i++)
			cash_balance_text[i] = Text(Text::Font::A11B, Text::Alignment::RIGHT, Color::Name::EMPEROR);

		selected_item = -1;
		preview_name = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, "", 220);
		preview_desc = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, "", 220);
		preview_price = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::SUPERNOVA);

		// Your character lives on the preview stage; selecting an equip
		// dresses it, arrow keys walk it, jump key jumps
		preview_look = player.get_look();
		preview_look.set_stance(Stance::Id::STAND1);
		cur_stance = Stance::Id::STAND1;

		active_subpanel = SUBPANEL_NONE;

		dimension = Texture(backgrnd).get_dimensions();
	}

	void UICashShop::draw(float inter) const
	{
		// A plain dark ground rather than the window's own picture. That
		// picture had every panel drawn into it at a fixed place, which is
		// what made this layout impossible to change and left nowhere to put
		// the tabs.
		GraphicsGL::get().drawrectangle(
			position.x(), position.y(), 1024, 768, 0.06f, 0.07f, 0.10f, 1.0f);

		// LEFT: who you are and what you own.
		draw_panel(position + Point<int16_t>(LEFT_X, PREVIEW_Y), LEFT_W, PREVIEW_H, "PREVIEW");

		// The scene behind the character, inside the preview panel.
		preview_scene[preview_index].draw(DrawArgument(
			position + Point<int16_t>(LEFT_X + 4, PREVIEW_Y + 24),
			Point<int16_t>(LEFT_W - 8, PREVIEW_H - 28)));

		draw_panel(position + Point<int16_t>(LEFT_X, WARDROBE_Y), LEFT_W, WARDROBE_H, "CASH ITEM WARDROBE");
		draw_panel(position + Point<int16_t>(LEFT_X, INVENTORY_Y), LEFT_W, INVENTORY_H, "ITEM INVENTORY");

		// RIGHT: the tab row, then what is for sale.
		for (uint8_t c = 0; c < CAT_COUNT; c++)
		{
			int16_t tx = RIGHT_X + c * (TAB_W + 2);
			bool here = (c == current_category);

			GraphicsGL::get().drawrectangle(
				position.x() + tx, position.y() + TAB_Y, TAB_W, TAB_H,
				here ? 0.30f : 0.12f, here ? 0.32f : 0.13f, here ? 0.40f : 0.17f, 1.0f);

			tab_label.change_text(category_name(static_cast<Category>(c)));
			tab_label.draw(position + Point<int16_t>(tx + TAB_W / 2, TAB_Y + 4));
		}

		draw_panel(position + Point<int16_t>(RIGHT_X, GRID_Y - 6),
			RIGHT_W, GRID_ROWS * STRIDE_Y + 12, "");

		UIElement::draw_sprites(inter);

		// One wallet. Only NX credit is ever spent here - every purchase is
		// sent as currency 1 - and the other two are always nought on this
		// server, so three numbers only invited the confusion they caused.
		wallet_label.change_text("Maple Points   " + std::to_string(get_cash_balance(0)));
		wallet_label.draw(position + Point<int16_t>(LEFT_X + 8, WALLET_Y));

		Point<int16_t> label_pos = position + Point<int16_t>(4, 3);
		job_label.draw(label_pos);

		size_t length = job_label.width();
		name_label.draw(label_pos + Point<int16_t>(length + 10, 0));

		if (filtered.empty())
			item_none.draw(position + Point<int16_t>(RIGHT_X + RIGHT_W / 2 - 166, GRID_Y + 180), inter);

		for (size_t i = 0; i < MAX_ITEMS; i++)
		{
			int16_t index = i + list_offset;

			if (index < static_cast<int16_t>(filtered.size()))
			{
				div_t div = std::div(i, GRID_COLS);
				Point<int16_t> cell = position + Point<int16_t>(GRID_X + STRIDE_X * div.rem, GRID_Y + STRIDE_Y * div.quot);
				Item item = items[filtered[index]];

				item_base.draw(cell, inter);
				// Icon centered in the card's upper frame; v83 icons carry
				// a (0,32) origin which doubles at 2x scale
				item.draw(DrawArgument(cell + Point<int16_t>(27, 101), 2.0f, 2.0f));

				item_name[i].draw(cell + Point<int16_t>(55, 108));
				item_price[i].draw(cell + Point<int16_t>(58, 127));
			}
		}

		// Your character on the stage, dressed with whatever is selected
		preview_look.draw(
			DrawArgument(position + Point<int16_t>(static_cast<int16_t>(char_x), static_cast<int16_t>(275.0f + char_yoff)), !facing_right),
			inter);

		if (selected_item >= 0 && selected_item < static_cast<int16_t>(items.size()))
		{
			const Item& sel = items[selected_item];
			Point<int16_t> pv = position + Point<int16_t>(652, 46);

			preview_name.change_text(sel.get_name());
			preview_name.draw(pv);

			std::string pricestr = std::to_string(sel.get_price()) + " NX";

			if (sel.count > 1)
				pricestr += " (" + std::to_string(sel.count) + ")";

			preview_price.change_text(pricestr);
			preview_price.draw(pv + Point<int16_t>(0, 16));
		}

		list_slider.draw(position);

		UIElement::draw_buttons(inter);

		// Draw active sub-panel (English popups only).
		switch (active_subpanel)
		{
		case SUBPANEL_GIFT:
			if (gift_bg.is_valid())
				gift_bg.draw(DrawArgument(position + Point<int16_t>(200, 50)));
			break;
		case SUBPANEL_COUPON:
			if (coupon_bg.is_valid())
				coupon_bg.draw(DrawArgument(position + Point<int16_t>(200, 50)));
			break;
		case SUBPANEL_PURCHASE:
			if (purchase_bg.is_valid())
				purchase_bg.draw(DrawArgument(position + Point<int16_t>(200, 50)));
			break;
		default:
			break;
		}

		// Player's cash items rendered into the baked CASH INVENTORY
		// panel in the right column (7x2 slot grid)
		const Inventory& inventory = Stage::get().get_player().get_inventory();
		uint8_t slotmax = inventory.get_slotmax(InventoryType::Id::CASH);
		int16_t shown = 0;

		for (int16_t slot = 1; slot <= slotmax && shown < 14; slot++)
		{
			int32_t item_id = inventory.get_item_id(InventoryType::Id::CASH, slot);

			if (item_id == 0)
				continue;

			div_t sdiv = std::div(shown, 7);
			Point<int16_t> slot_pos = position + Point<int16_t>(771 + 34 * sdiv.rem, 513 + 34 * sdiv.quot);

			// v83 item icons carry a (0, 32) origin, so draw at the
			// slot's bottom edge to land the 32x32 icon inside it.
			ItemData::get(item_id).get_icon(false).draw(DrawArgument(slot_pos + Point<int16_t>(0, 32)));

			shown++;
		}
	}

	void UICashShop::send_key(int32_t keycode, bool pressed, bool escape)
	{
		switch (keycode)
		{
		case KeyAction::Id::LEFT:
			key_left = pressed;
			break;
		case KeyAction::Id::RIGHT:
			key_right = pressed;
			break;
		case KeyAction::Id::JUMP:
			if (pressed && !char_jumping)
			{
				char_jumping = true;
				char_vy = -6.2f;
			}
			break;
		default:
			break;
		}
	}

	void UICashShop::update()
	{
		UIElement::update();

		preview_look.update(Constants::TIMESTEP);

		// Walk
		float dx = 0.0f;

		if (key_left)
			dx -= 1.3f;

		if (key_right)
			dx += 1.3f;

		if (dx != 0.0f)
		{
			char_x += dx;
			facing_right = dx > 0.0f;

			if (char_x < 662.0f)
				char_x = 662.0f;

			if (char_x > 992.0f)
				char_x = 992.0f;
		}

		// Jump arc
		if (char_jumping)
		{
			char_yoff += char_vy;
			char_vy += 0.28f;

			if (char_yoff >= 0.0f)
			{
				char_yoff = 0.0f;
				char_vy = 0.0f;
				char_jumping = false;
			}
		}

		// Stance follows the movement state
		uint8_t want = char_jumping ? Stance::Id::JUMP
			: (dx != 0.0f) ? Stance::Id::WALK1
			: Stance::Id::STAND1;

		if (want != cur_stance)
		{
			cur_stance = want;
			preview_look.set_stance(static_cast<Stance::Id>(cur_stance));
		}
	}

	Button::State UICashShop::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::BtPreview1:
		case Buttons::BtPreview2:
		case Buttons::BtPreview3:
			buttons[preview_index]->set_state(Button::State::NORMAL);

			preview_index = buttonid;
			return Button::State::PRESSED;
		case Buttons::BtExit:
		{
			uint16_t width = Setting<Width>::get().load();
			uint16_t height = Setting<Height>::get().load();

			// Restore the user's pre-cashshop UI scale (transition() forced
			Constants::Constants::get().set_viewwidth(width);
			Constants::Constants::get().set_viewheight(height);

			float fadestep = 0.025f;

			Window::get().fadeout(
				fadestep,
				[]()
				{
					GraphicsGL::get().clear();
					ChangeMapPacket(false, Stage::get().get_mapid(), "", false).dispatch();
				}
			);

			GraphicsGL::get().lock();
			Stage::get().clear();
			Timer::get().start();

			return Button::State::NORMAL;
		}
		case Buttons::BtNext:
		{
			size_t size = promotion_sprites.size() - 1;

			promotion_index++;

			if (promotion_index > size)
				promotion_index = 0;

			return Button::State::NORMAL;
		}
		case Buttons::BtPrev:
		{
			size_t size = promotion_sprites.size() - 1;

			promotion_index--;

			if (promotion_index < 0)
				promotion_index = size;

			return Button::State::NORMAL;
		}
		case Buttons::BtChargeNX:
		{
			std::string url = "";

#ifdef _WIN32
			ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif

			return Button::State::NORMAL;
		}
		case Buttons::BtCoupon:
			active_subpanel = (active_subpanel == SUBPANEL_COUPON) ? SUBPANEL_NONE : SUBPANEL_COUPON;
			return Button::State::NORMAL;
		default:
			break;
		}

		if (buttonid >= Buttons::BtBuy)
		{
			int16_t index = buttonid - Buttons::BtBuy + list_offset;

			if (index >= 0 && index < static_cast<int16_t>(filtered.size()))
			{
				Item item = items[filtered[index]];

				// Currency type 1 = NX Credit (standard purchase)
				int8_t currency = 1;
				BuyCashItemPacket(currency, item.sn).dispatch();
			}

			return Button::State::NORMAL;
		}

		return Button::State::DISABLED;
	}

	Cursor::State UICashShop::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Point<int16_t> cursor_relative = cursorpos - position;

		if (list_slider.isenabled())
		{
			Cursor::State state = list_slider.send_cursor(cursor_relative, clicked);

			if (state != Cursor::State::IDLE)
				return state;
		}

		if (clicked)
		{
			Point<int16_t> off = cursorpos - position;

			// The tab row first: it sits above the cards and picking a tab
			// changes what they show, so it has to be tested before them.
			for (uint8_t c = 0; c < CAT_COUNT; c++)
			{
				Rectangle<int16_t> tab(
					Point<int16_t>(RIGHT_X + c * (TAB_W + 2), TAB_Y),
					Point<int16_t>(RIGHT_X + c * (TAB_W + 2) + TAB_W, TAB_Y + TAB_H));

				if (!tab.contains(off))
					continue;

				if (clicked && current_category != c)
				{
					current_category = static_cast<Category>(c);

					rebuild_filter();
					update_items();

					Sound(Sound::Name::TAB).play();
				}

				return Cursor::State::CANCLICK;
			}

			for (int16_t i = 0; i < MAX_ITEMS; i++)
			{
				int16_t index = i + list_offset;

				if (index >= static_cast<int16_t>(filtered.size()))
					break;

				div_t cdiv = std::div(i, GRID_COLS);
				Rectangle<int16_t> card(
					Point<int16_t>(GRID_X + STRIDE_X * cdiv.rem, GRID_Y + STRIDE_Y * cdiv.quot),
					Point<int16_t>(GRID_X + STRIDE_X * cdiv.rem + CARD_W, GRID_Y + STRIDE_Y * cdiv.quot + CARD_H)
				);

				if (card.contains(off))
				{
					// The REAL index, not the position in the grid - the grid
					// only shows a page of whichever tab is open.
					selected_item = static_cast<int16_t>(filtered[index]);

					int32_t itemid = items[selected_item].get_itemid();

					if (itemid >= 1000000 && itemid < 2000000)
					{
						// Try-on: current look plus the selected equip
						preview_look = Stage::get().get_player().get_look();
						preview_look.add_equip(itemid);
						preview_look.set_stance(static_cast<Stance::Id>(cur_stance));
					}

					break;
				}
			}
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}

	UIElement::Type UICashShop::get_type() const
	{
		return TYPE;
	}

	void UICashShop::exit_cashshop()
	{
		UI& ui = UI::get();
		ui.change_state(UI::State::GAME);

		// Restore UI scale + physical window size that were overridden in
		// SetCashShopHandler::transition(). Without this the game world would
		// remain confined to the 1024x768 cash-shop window at scale 1.0.
		uint16_t width = Setting<Width>::get().load();
		uint16_t height = Setting<Height>::get().load();
		Constants::Constants::get().set_viewwidth(width);
		Constants::Constants::get().set_viewheight(height);

		Stage& stage = Stage::get();
		Player& player = stage.get_player();

		PlayerLoginPacket(player.get_oid()).dispatch();

		int32_t mapid = player.get_stats().get_mapid();
		uint8_t portalid = player.get_stats().get_portal();

		stage.load(mapid, portalid);

		ui.enable();
		Timer::get().start();
		GraphicsGL::get().unlock();
	}

	void UICashShop::update_items()
	{
		for (size_t i = 0; i < MAX_ITEMS; i++)
		{
			int16_t index = i + list_offset;
			bool found_item = index < static_cast<int16_t>(filtered.size());

			buttons[Buttons::BtBuy + i]->set_active(found_item);

			std::string name = "";
			std::string price_text = "";
			std::string discount_text = "";
			std::string percent_text = "";

			if (found_item)
			{
				Item item = items[filtered[index]];

				name = item.get_name();

				int32_t price = item.get_price();
				price_text = std::to_string(price);

				if (item.discount_price > 0 && price > 0)
				{
					discount_text = price_text;

					uint32_t discount = item.discount_price;
					price_text = std::to_string(discount);

					float_t percent = (float)discount / price;
					std::string percent_str = std::to_string(percent);
					percent_text = "(" + percent_str.substr(2, 1) + "%)";
				}

				string_format::split_number(price_text);
				string_format::split_number(discount_text);

				price_text += " NX";

				if (discount_text != "")
					discount_text += " NX";

				if (item.count > 0)
					price_text += "(" + std::to_string(item.count) + ")";
			}

			item_name[i].change_text(name);
			item_price[i].change_text(price_text);
			item_discount[i].change_text(discount_text);
			item_percent[i].change_text(percent_text);

		}
	}
}