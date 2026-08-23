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

#include "../PacketHandler.h"

#include <cstdint>
#include <vector>

namespace ms
{
	// One thing sitting in the cash shop's locker.
	//
	// A bought item does NOT go into the character's inventory - it goes
	// into a per-account locker the shop keeps, and has to be taken out
	// before it can be worn or used. That is why a purchase that plainly
	// succeeded still left nothing to show for it anywhere on the character.
	struct CashLockerItem
	{
		int64_t cashid;
		int32_t itemid;
		int16_t quantity;
	};

	// What the locker held when the server last said so.
	const std::vector<CashLockerItem>& get_cash_locker();

	// The take-out reply (0x68) does not say WHICH item came out, so the
	// asker records it here before sending the request.
	void set_pending_cash_take(int64_t cashid);
	// Handler for entering the Cash Shop
	class SetCashShopHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;

	private:
		void transition() const;
	};

	// Returns the UI scale that was active immediately before the cash shop
	// transition. The cash shop layout is authored at 1:1 for a 1024x768
	// window, so the transition forces UI_SCALE to 1.0. Call this on exit
	// to restore the user's previous scale.
	float get_pre_cashshop_ui_scale();

	// Last cash balances the server reported (QUERY_CASH_RESULT):
	// 0 = NX credit, 1 = maple points, 2 = NX prepaid
	int32_t get_cash_balance(int which);

	// Handler for Cash Shop operation responses (buy, coupon, etc.)
	class CashShopOperationHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;
	};

	// Handler for entering MTS (SET_ITC, opcode 126)
	class SetITCHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;
	};

	// Cash shop cash query result
	class QueryCashResultHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Cash shop name change check
	class CashShopNameChangeHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Cash shop name change possible result
	class CashShopNameChangePossibleHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Cash shop world transfer possible result
	class CashShopTransferWorldHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Cash shop gachapon item result
	class CashGachaponResultHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};
}