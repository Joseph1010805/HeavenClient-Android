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

#include "../UIElement.h"

#include "../Components/Charset.h"
#include "../Components/Gauge.h"
#include "../Components/Slider.h"

#include "../Character/Look/CharLook.h"
#include "../Data/ItemData.h"
#include "../Graphics/Text.h"

namespace ms
{
	class UICashShop : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::CASHSHOP;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UICashShop();

			void draw(float inter) const;
		void update() override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		Button::State button_pressed(uint16_t buttonid);

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		void exit_cashshop();

		// Say something across the top of the shop for a few seconds.
		//
		// The shop cannot use UIStatusMessenger: that lives in the GAME UI
		// state and this is the CASHSHOP one, so every message sent to it
		// while the shop was open went nowhere - which is a large part of
		// why buying appeared to do nothing.
		void show_message(Color::Name color, const std::string& text);

	private:
		void update_items();

		// The shop is laid out by hand rather than stamped from the window's
		// own picture.
		//
		// That picture is a single 1024x768 bitmap with every panel - the
		// preview, the inventories, the balance labels - already drawn in
		// place, so nothing could be moved and there was nowhere to put a row
		// of category tabs. Drawing it ourselves is the same approach the
		// inventory and character pages take, and for the same reason.
		//
		// Left column: the character and what they own. Right: what is for
		// sale. Modelled on the classic shop rather than this UI version's.
		//
		// The window's own buttons (exit, help, coupon) carry absolute
		// positions in their origins and land in a band across the top at
		// y 10-30, so everything below starts at 34.
		static constexpr int16_t PAD = 8;

		static constexpr int16_t LEFT_X = PAD;
		static constexpr int16_t LEFT_W = 292;

		static constexpr int16_t PREVIEW_Y = 34;
		static constexpr int16_t PREVIEW_H = 296;

		static constexpr int16_t SELECTED_Y = PREVIEW_Y + PREVIEW_H + PAD;
		static constexpr int16_t SELECTED_H = 108;

		static constexpr int16_t INVENTORY_Y = SELECTED_Y + SELECTED_H + PAD;
		static constexpr int16_t INVENTORY_H = 244;

		// The tab row and the grid of things to buy.
		static constexpr int16_t RIGHT_X = LEFT_X + LEFT_W + PAD;
		static constexpr int16_t RIGHT_W = 1024 - RIGHT_X - PAD;

		static constexpr int16_t TAB_Y = 34;
		static constexpr int16_t TAB_H = 26;
		static constexpr int16_t TAB_W = 88;

		static constexpr uint8_t GRID_COLS = 4u;
		static constexpr uint8_t GRID_ROWS = 3u;
		static constexpr uint8_t MAX_ITEMS = GRID_COLS * GRID_ROWS;
		static constexpr int16_t GRID_X = RIGHT_X + 12;
		static constexpr int16_t GRID_Y = TAB_Y + TAB_H + 14;
		static constexpr int16_t STRIDE_X = 170;
		static constexpr int16_t STRIDE_Y = 200;
		static constexpr int16_t CARD_W = 119;
		static constexpr int16_t CARD_H = 184;

		// The locker grid inside MY CASH ITEMS: 32x32 icons on a 36px pitch,
		// centred in the panel.
		static constexpr int16_t SLOT_COLS = 7;
		static constexpr int16_t SLOT_PITCH = 36;
		static constexpr int16_t SLOT_X = LEFT_X + (LEFT_W - SLOT_COLS * SLOT_PITCH) / 2;
		static constexpr int16_t SLOT_Y = INVENTORY_Y + 26;
		static constexpr int16_t SLOT_ROWS = (INVENTORY_H - 30) / SLOT_PITCH;

		static Point<int16_t> locker_slot(int16_t n);

		// Where the character stands inside the preview panel, and how far
		// left and right the arrow keys may walk them without leaving it.
		static constexpr int16_t STAGE_X = LEFT_X + LEFT_W / 2;
		static constexpr int16_t STAGE_Y = PREVIEW_Y + PREVIEW_H - 40;
		static constexpr int16_t STAGE_MIN_X = LEFT_X + 40;
		static constexpr int16_t STAGE_MAX_X = LEFT_X + LEFT_W - 40;

		// One wallet, not three. The server keeps NX credit, maple points and
		// NX prepaid separately, but only NX credit is ever spent here (every
		// purchase is sent as currency 1) and the other two stay at zero on
		// this server unless a coupon is redeemed. Showing three numbers where
		// two are always nought invites exactly the confusion it caused: a
		// balance of 100 read as maple points when it was NX.
		static constexpr int16_t WALLET_Y = INVENTORY_Y + INVENTORY_H + PAD;

		// What the tabs sort by. MapleStory encodes an item's kind in the
		// leading digits of its id, which is the only category information
		// Commodity.img carries.
		enum Category : uint8_t
		{
			CAT_ALL,
			CAT_HAT,
			CAT_FACE,
			CAT_CLOTHES,
			CAT_WEAPON,
			CAT_PET,
			CAT_OTHER,
			CAT_COUNT
		};

		static const char* category_name(Category c);
		static Category category_of(int32_t itemid);

		Category current_category = CAT_ALL;

		// Set once the exit packet is away. Escape and the exit button both
		// reach exit_cashshop(), and asking twice would leave the server
		// answering a request from a client that had already left.
		bool exiting = false;

		// Every item on sale, and the subset the chosen tab shows.
		std::vector<size_t> filtered;

		void rebuild_filter();
		void draw_panel(Point<int16_t> at, int16_t w, int16_t h, const char* title) const;

		mutable Text panel_title;
		mutable Text tab_label;
		mutable Text wallet_label;

		Text status_label;
		int16_t status_ticks = 0;

		class Item
		{
		public:
			enum Label : uint8_t
			{
				ACTION,
				BOMB_SALE,
				BONUS,
				EVENT = 4,
				HOT,
				LIMITED,
				LIMITED_BRONZE,
				LIMITED_GOLD,
				LIMITED_SILVER,
				LUNA_CRYSTAL,
				MASTER = 12,
				MUST,
				NEW,
				SALE = 17,
				SPEICAL,
				SPECIAL_PRICE,
				TIME,
				TODAY,
				WEEKLY,
				WONDER_BERRY,
				WORLD_SALE,
				NONE
			};

			// price is the NX cash price from Commodity.img, not the
			// item's meso shop price
			Item(int32_t itemid, int32_t sn, Label label, int32_t price, uint16_t count) : sn(sn), label(label), price(price), discount_price(0), count(count), data(ItemData::get(itemid)) {}

			int32_t sn;
			Label label;
			int32_t price;
			int32_t discount_price;
			uint16_t count;

			void draw(const DrawArgument& args) const
			{
				data.get_icon(false).draw(args);
			}

			const std::string get_name() const
			{
				return data.get_name();
			}

			const std::string& get_desc() const
			{
				return data.get_desc();
			}

			int32_t get_itemid() const
			{
				return data.get_id();
			}

			const int32_t get_price() const
			{
				return price;
			}

		private:
			const ItemData& data;
		};

		enum Buttons : uint16_t
		{
			BtPreview1,
			BtPreview2,
			BtPreview3,
			BtExit,
			BtChargeNX,
			BtChargeRefresh,
			BtMileage,
			BtHelp,
			BtCoupon,
			BtNext,
			BtPrev,
			BtDetailPackage,
			BtNonGrade,
			BtBuyAvatar,
			BtDefaultAvatar,
			BtSaveAvatar,
			BtTakeoffAvatar,
			BtBuy
		};

		Point<int16_t> BestNew_dim;

		Sprite preview_sprites[3];
		uint8_t preview_index;

		Sprite menu_tabs[9];
		uint8_t menu_index;

		Text job_label;
		Text name_label;
		mutable Text cash_balance_text[3];

		int16_t selected_item;
		CharLook preview_look;

		// Controllable stage character
		float char_x = static_cast<float>(STAGE_X);
		float char_yoff = 0.0f;
		float char_vy = 0.0f;
		bool char_jumping = false;
		bool key_left = false;
		bool key_right = false;
		bool facing_right = true;
		uint8_t cur_stance = 0;

		Texture preview_scene[3];
		mutable Text preview_name;
		mutable Text preview_desc;
		mutable Text preview_price;

		std::vector<Sprite> promotion_sprites;
		Point<int16_t> promotion_pos;
		int8_t promotion_index;

		Sprite mvp_sprites[7];
		Point<int16_t> mvp_pos;
		uint8_t mvp_grade;
		Gauge mvp_gauge;
		float_t mvp_exp;

		Charset charge_charset;

		Sprite item_base;
		Sprite item_line;
		Sprite item_none;
		std::vector<Sprite> item_labels;
		std::vector<Item> items;
		Text item_name[MAX_ITEMS];
		Text item_price[MAX_ITEMS];
		Text item_discount[MAX_ITEMS];
		Text item_percent[MAX_ITEMS];

		Slider list_slider;
		int16_t list_offset;

		// Sub-panel backgrounds
		Texture wishlist_bg;
		Texture gift_bg;
		Texture coupon_bg;
		Texture search_bg;
		Texture cs_inventory_bg;
		Texture purchase_bg;

		// Active sub-panel tracking
		enum SubPanel : uint8_t
		{
			SUBPANEL_NONE,
			SUBPANEL_WISHLIST,
			SUBPANEL_GIFT,
			SUBPANEL_COUPON,
			SUBPANEL_SEARCH,
			SUBPANEL_INVENTORY,
			SUBPANEL_PURCHASE
		};
		SubPanel active_subpanel;
	};
}