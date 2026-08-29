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

#include "UINotice.h"

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

	// Whether an equip has anything the character renderer can actually draw.
	//
	// `ItemData::is_valid` only asks whether the item has an `info` node, and
	// that is a weaker test than it looks: an item can have an icon, a name
	// and a price - so it shows a perfectly ordinary card and takes the money
	// - while carrying no body art at all, in which case wearing it does
	// nothing and there is no way to tell from inside the game. A handful of
	// entries in Commodity.img are like that. `tools/shop_audit.py` lists
	// them; this is the check that keeps them off the shelf.
	//
	// Non-equips are not asked: a potion has no body art and is not supposed
	// to. Nor is the deliberately empty transparent hat, which is invisible
	// on purpose.
	bool UICashShop::has_wearable_art(int32_t itemid)
	{
		if (itemid < 1000000 || itemid >= 2000000)
			return true;

		// The transparent hat is empty on purpose - it is what you wear to
		// show no hat at all. Clothing.cpp knows it by the same number.
		if (itemid == 1002186)
			return true;

		const ItemData& data = ItemData::get(itemid);

		nl::node src = nl::nx::character[data.get_category()]["0" + std::to_string(itemid) + ".img"];

		for (nl::node child : src)
			if (child.name() != "info")
				return true;

		return false;
	}

	void UICashShop::rebuild_filter()
	{
		filtered.clear();

		for (size_t i = 0; i < items.size(); i++)
			if (current_category == CAT_ALL
				|| category_of(items[i].get_itemid()) == current_category)
				filtered.push_back(i);

		list_offset = 0;

		// The bar has to shrink with the tab, or a category holding one page
		// still scrolls through the twenty the full catalogue has.
		int16_t rowmax = static_cast<int16_t>((filtered.size() + GRID_COLS - 1) / GRID_COLS);
		list_slider.setrows(0, GRID_ROWS, rowmax > 0 ? rowmax : 1);
	}

	// A plain translucent plate with a heading, standing in for the frames the
	// window's own artwork used to supply.
	void UICashShop::draw_panel(Point<int16_t> at, int16_t w, int16_t h, const char* title) const
	{
		GraphicsGL::get().drawrectangle(at.x(), at.y(), w, h, 0.0f, 0.0f, 0.0f, 0.45f);

		if (!title || !*title)
			return;

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
		nl::node Preview = Base["Preview"];
		nl::node CSList = CashShop["CSList"];
		nl::node MainItem = CashShopGL["MainMenu"]["MainItem"];
		nl::node Popup = CashShopGL["Popup"];

		// The window's own 1024x768 picture is deliberately NOT added.
		//
		// It has every panel of the old layout painted into it, so adding it
		// meant `UIElement::draw_sprites` stamped the whole original shop
		// over the top of the panels drawn below - which is exactly what it
		// did: no tabs were visible, and the cards appeared to spill over a
		// preview frame that was part of the picture rather than part of the
		// layout. There is nothing left to keep of it.

		// BestNew banner has Korean text baked in — omit. Leave dimensions
		// zeroed out (item_none fallback still works).
		BestNew_dim = Point<int16_t>();

		for (size_t i = 0; i < 3; i++)
		{
			preview_sprites[i] = Preview[i];
			preview_scene[i] = Texture(Preview[i]);
		}

		// The three backdrop pickers, tucked into the preview panel's own
		// bottom-right corner rather than left out at 957 where the old
		// picture had them.
		for (size_t i = 0; i < 3; i++)
			buttons[Buttons::BtPreview1 + i] = std::make_unique<TwoSpriteButton>(Base["Tab"]["Disable"][i], Base["Tab"]["Enable"][i], Point<int16_t>(LEFT_X + LEFT_W - 62 + (i * 17), PREVIEW_Y + 4));

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

		// The search bar's background is likewise a fixed-position piece of
		// the old layout, and there is no textfield behind it to search
		// with. Left out until there is.

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

			if (!has_wearable_art(itemid))
				continue;

			int32_t sn = entry["SN"];
			int32_t price = entry["Price"];
			int32_t count = entry["Count"];

			items.push_back(Item(itemid, sn, Item::Label::NONE, price, static_cast<uint16_t>(count)));
		}

		panel_title = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::WHITE);
		tab_label = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE);
		wallet_label = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::SUPERNOVA);

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

		// Hard against the right edge of the grid panel. It used to sit at
		// 608, which is the middle of the fourth column.
		Point<int16_t> slider_pos = Point<int16_t>(RIGHT_X + RIGHT_W - 20, GRID_Y);

		list_slider = Slider(
			Slider::Type::DEFAULT,
			Range<int16_t>(slider_pos.y(), slider_pos.y() + GRID_ROWS * STRIDE_Y - 30),
			slider_pos.x(),
			GRID_ROWS,
			static_cast<int16_t>(items.size() / GRID_COLS + 1),
			[&](bool upwards)
			{
				int16_t shift = upwards ? -GRID_COLS : GRID_COLS;
				bool above = list_offset + shift >= 0;
				bool below = list_offset + shift < static_cast<int16_t>(filtered.size());

				if (above && below)
				{
					list_offset += shift;

					update_items();
				}
			}
		);

		// After the slider exists - rebuild_filter() sizes it.
		rebuild_filter();
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

		// No Charge NX button. It opened a web page to buy cash with real
		// money, which this server has no notion of and which the handler
		// behind it never had a URL for - a button that could only ever do
		// nothing. NX is earned from drops here.

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

		// The shop owns the whole 1024x768 canvas; there is no bitmap left to
		// measure it from.
		dimension = Point<int16_t>(1024, 768);
	}

	void UICashShop::draw(float inter) const
	{
		// A plain dark ground rather than the window's own picture. That
		// picture had every panel drawn into it at a fixed place, which is
		// what made this layout impossible to change and left nowhere to put
		// the tabs.
		GraphicsGL::get().drawrectangle(
			position.x(), position.y(), 1024, 768, 0.06f, 0.07f, 0.10f, 1.0f);

		// LEFT: who you are, what you have picked up, and what you own.
		draw_panel(position + Point<int16_t>(LEFT_X, PREVIEW_Y), LEFT_W, PREVIEW_H, "PREVIEW");

		// The scene behind the character, inside the preview panel.
		preview_scene[preview_index].draw(DrawArgument(
			position + Point<int16_t>(LEFT_X + 4, PREVIEW_Y + 22),
			Point<int16_t>(LEFT_W - 8, PREVIEW_H - 26)));

		// Your character on the stage, dressed with whatever is selected.
		// Drawn here, between the backdrop and the panels below it, so
		// nothing can end up standing on top of the tabs.
		preview_look.draw(
			DrawArgument(position + Point<int16_t>(static_cast<int16_t>(char_x), static_cast<int16_t>(STAGE_Y + char_yoff))),
			inter);

		draw_panel(position + Point<int16_t>(LEFT_X, SELECTED_Y), LEFT_W, SELECTED_H, "SELECTED");
		draw_panel(position + Point<int16_t>(LEFT_X, INVENTORY_Y), LEFT_W, INVENTORY_H, "INFORMATION");

		if (selected_item >= 0 && selected_item < static_cast<int16_t>(items.size()))
		{
			const Item& sel = items[selected_item];
			Point<int16_t> pv = position + Point<int16_t>(LEFT_X + 8, SELECTED_Y + 26);

			preview_name.change_text(sel.get_name());
			preview_name.draw(pv);

			std::string pricestr = std::to_string(sel.get_price()) + " NX";

			if (sel.count > 1)
				pricestr += "  x" + std::to_string(sel.count);

			preview_price.change_text(pricestr);
			preview_price.draw(pv + Point<int16_t>(0, preview_name.height()));
		}

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

		draw_panel(position + Point<int16_t>(RIGHT_X, GRID_Y - 8),
			RIGHT_W, GRID_ROWS * STRIDE_Y + 16, "");

		// One wallet. Only NX credit is ever spent here - every purchase is
		// sent as currency 1 - and the other two are always nought on this
		// server, so three numbers only invited the confusion they caused.
		wallet_label.change_text("Maple Points   " + std::to_string(get_cash_balance(0)));
		wallet_label.draw(position + Point<int16_t>(LEFT_X + 8, WALLET_Y));

		Point<int16_t> label_pos = position + Point<int16_t>(4, 3);
		job_label.draw(label_pos);

		size_t length = job_label.width();
		name_label.draw(label_pos + Point<int16_t>(length + 10, 0));

		// What the shop last had to say, centred over the grid.
		if (status_ticks > 0)
			status_label.draw(position + Point<int16_t>(RIGHT_X + RIGHT_W / 2, 6));

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

		// THE INFORMATION PANEL.
		//
		// This was MY CASH ITEMS - a grid of what sat in the account locker.
		// It is gone for two reasons: a purchase now goes straight to the
		// character's inventory rather than resting in a locker, so the grid
		// had nothing to show; and the thing actually wanted while shopping is
		// what an item IS, which was nowhere on the screen.
		//
		// Same detail the inventory shows on hover - icon, category and the
		// item's own description text - drawn in the column rather than in a
		// floating tooltip, because on a handheld there is no hover to keep a
		// tooltip alive.
		if (selected_item >= 0 && selected_item < static_cast<int16_t>(items.size()))
		{
			const Item& sel = items[selected_item];
			const ItemData& data = ItemData::get(sel.get_itemid());

			Point<int16_t> at = position + Point<int16_t>(LEFT_X + 12, INVENTORY_Y + 30);

			if (data.is_valid())
			{
				// v83 item icons carry a (0, 32) origin, so drawing at the top
				// edge would put the picture above the panel.
				data.get_icon(false).draw(DrawArgument(at + Point<int16_t>(0, 32)));

				panel_title.change_text(data.get_category());
				panel_title.draw(at + Point<int16_t>(42, 0));

				// Re-wrapped only when the selection changes - laying out a
				// paragraph every frame is most of the cost of this panel.
				if (info_desc_for != sel.get_itemid())
				{
					info_desc_for = sel.get_itemid();

					std::string body = data.get_desc();

					if (body.empty())
						body = "No description.";

					info_desc = Text(Text::Font::A11M, Text::Alignment::LEFT,
						Color::Name::WHITE, body, LEFT_W - 24);
				}

				info_desc.draw(at + Point<int16_t>(0, 40));
			}
		}
		else
		{
			panel_title.change_text("Select an item to see what it does.");
			panel_title.draw(position + Point<int16_t>(LEFT_X + 12, INVENTORY_Y + 30));
		}
	}

	// Where the nth locker icon sits, relative to the shop's own origin.
	Point<int16_t> UICashShop::locker_slot(int16_t n)
	{
		div_t d = std::div(n, static_cast<int>(SLOT_COLS));

		return Point<int16_t>(SLOT_X + SLOT_PITCH * d.rem, SLOT_Y + SLOT_PITCH * d.quot);
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

		if (status_ticks > 0)
			status_ticks--;

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
			// WHICH WAY HE FACES, set on the look rather than at the draw.
			//
			// CharLook keeps its own `flip` and folds it into every part it
			// draws; passing another flip in the DrawArgument fought with it,
			// which is why he walked backwards. The world does it this way -
			// Char::set_direction - and this now matches.
			//
			// Only when actually moving: `dx > 0` was also asked while standing
			// still, so stopping turned him to face left every time.
			if (dx != 0.0f)
			{
				facing_right = dx > 0.0f;
				preview_look.set_direction(facing_right);
			}

			// The stage is the preview panel now, not the right-hand third
			// of the window the old picture gave it.
			if (char_x < static_cast<float>(STAGE_MIN_X))
				char_x = static_cast<float>(STAGE_MIN_X);

			if (char_x > static_cast<float>(STAGE_MAX_X))
				char_x = static_cast<float>(STAGE_MAX_X);
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

	void UICashShop::send_scroll(double yoffset)
	{
		// UIStateCashShop has always forwarded the wheel to whatever is in
		// front; this window simply had nothing to receive it, so the item
		// grid could only be dragged by its bar.
		if (list_slider.isenabled())
			list_slider.send_scroll(yoffset);
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
			exit_cashshop();
			return Button::State::NORMAL;
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

				// Say so when it cannot be afforded.
				//
				// Cosmic answers a purchase it refuses with nothing but a
				// fresh balance - `canBuy` fails and it calls
				// enableCSActions() - so an unaffordable item and a
				// successful one looked exactly alike from here: nothing
				// happened. The server still checks; this only makes the
				// commonest refusal visible.
				if (item.get_price() > get_cash_balance(0))
				{
					show_message(Color::Name::RED, "Not enough Maple Points for that.");

					return Button::State::NORMAL;
				}

				// ASK FIRST.
				//
				// A tap on a handheld is easy to make by accident, the BUY
				// buttons sit under your thumb while scrolling, and there is no
				// refund - so the one irreversible thing on this screen was the
				// easiest to do without meaning to.
				//
				// The name and the price are both in the question, because
				// "Buy this?" is not enough to catch the case that matters:
				// having selected one item and pressed BUY under another.
				int32_t sn = item.sn;

				std::string asking = "Buy " + item.get_name() + " for "
					+ std::to_string(item.get_price()) + " NX?";

				UI::get().emplace<UIYesNo>(asking,
					[sn](bool yes)
					{
						if (!yes)
							return;

						// Currency type 1 = NX Credit (standard purchase)
						int8_t currency = 1;
						BuyCashItemPacket(currency, sn).dispatch();
					});
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

			// A locker slot: tapping one takes that item out of the shop's
			// locker and puts it on the character.
			const std::vector<CashLockerItem>& locker = get_cash_locker();

			for (int16_t n = 0; n < static_cast<int16_t>(locker.size()) && n < SLOT_COLS * SLOT_ROWS; n++)
			{
				Point<int16_t> at = locker_slot(n);
				Rectangle<int16_t> slot(at, at + Point<int16_t>(32, 32));

				if (!slot.contains(off))
					continue;

				set_pending_cash_take(locker[n].cashid);
				TakeFromCashInventoryPacket(locker[n].cashid).dispatch();

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

	void UICashShop::show_message(Color::Name color, const std::string& text)
	{
		status_label = Text(Text::Font::A12B, Text::Alignment::CENTER, color, text);

		// About five seconds at the fixed timestep.
		status_ticks = 300;
	}

	void UICashShop::exit_cashshop()
	{
		// Ask, and wait to be told - do not walk out on our own.
		//
		// This used to change state to GAME and load the map locally without
		// telling the server anything, which left Cosmic still holding the
		// character in the shop and off the map. The other half of it sent a
		// full CHANGE_MAP, which Cosmic treats as a hack while the shop is
		// open and answers by disconnecting - the white screen.
		//
		// The real exit is an EMPTY change-map. The server replies with
		// CHANGE_CHANNEL and the whole return trip is driven from
		// ChangeChannelHandler: reconnect, log in, and the SET_FIELD that
		// follows puts the world back, restores the view size and returns
		// the UI to GAME. Nothing here should anticipate any of that.
		if (exiting)
			return;

		exiting = true;

		ExitCashShopPacket().dispatch();
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