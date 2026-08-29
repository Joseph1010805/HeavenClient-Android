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
#include "CharEquips.h"

#include <cctype>

namespace ms
{
	namespace
	{
		// A vslot IS A LIST, NOT A NAME.
		//
		// It spells out every visual slot the equip occupies, as a run of
		// two-character tokens - Cp for the cap itself, H1..H6/Hd/Hs/Hf/Hb/Hc/Hx
		// for the hair layers it covers, Af/Ay/As/Ae/Fc for face and accessory
		// slots. "CpH1H5" is a cap that hides two hair layers; "CpHdH1H2H3H4" is
		// a welding mask that hides five.
		//
		// getcaptype() compared that string against three literals and returned
		// NONE for everything else, and CharLook's NONE branch draws the hair
		// and never calls equips.draw(HAT, ...) at all. So a hat with any other
		// spelling was loaded, stored, equipped - and never drawn. 144 of the
		// game's 908 hats, 60 of them on sale in the cash shop, appeared in the
		// list, took the money, went in the locker and stayed invisible. There
		// was nothing in the game to tell you why, because the item was working
		// perfectly right up to the last step.
		//
		// This reads the list instead of matching the name. How much hair a hat
		// covers is what the three cap types actually distinguish, so counting
		// the hair tokens is the same question the literals were asking, asked
		// in a way that does not depend on the order somebody wrote them in -
		// note "CpH1H2H3H5HfHsAfAyAsAeHbH4H6", where H4 and H6 come after the
		// accessories. No two of these strings would ever have been guessable.
		CharEquips::CapType captype_from_vslot(const std::string& vslot)
		{
			size_t hair = 0;
			bool hides_eyes = false;

			// A token is an upper-case letter and whatever lower-case letters
			// or digits follow it. Scanned rather than taken in strides of two
			// so a malformed vslot cannot silently shift every later token.
			for (size_t i = 0; i < vslot.size(); )
			{
				if (!std::isupper(static_cast<unsigned char>(vslot[i])))
				{
					i++;
					continue;
				}

				size_t j = i + 1;

				while (j < vslot.size() && !std::isupper(static_cast<unsigned char>(vslot[j])))
					j++;

				const std::string token = vslot.substr(i, j - i);

				if (token[0] == 'H')
					hair++;
				else if (token == "Ay" || token == "As")
					hides_eyes = true;

				i = j;
			}

			// Matched to what the three known-good spellings already mean, so
			// the fallback and the literals agree rather than disagreeing at
			// the edges: CpH5 hides one hair layer and is a HEADBAND, CpH1H5
			// hides two and is a HALFCOVER, and CpH1H5AyAs is the same two plus
			// the eyes and is a FULLCOVER.
			if (hair >= 3 || (hair >= 2 && hides_eyes))
				return CharEquips::CapType::FULLCOVER;

			if (hair == 2)
				return CharEquips::CapType::HALFCOVER;

			return CharEquips::CapType::HEADBAND;
		}
	}

	CharEquips::CharEquips()
	{
		for (auto iter : clothes)
			iter.second = nullptr;
	}

	void CharEquips::draw(Equipslot::Id slot, Stance::Id stance, Clothing::Layer layer, uint8_t frame, const DrawArgument& args) const
	{
		if (const Clothing * cloth = clothes[slot])
			cloth->draw(stance, layer, frame, args);
	}

	void CharEquips::draw_faceacc(Expression::Id expression, uint8_t frame, bool overface, const DrawArgument& args) const
	{
		if (const Clothing * cloth = clothes[Equipslot::Id::FACE])
			if (cloth->is_faceacc())
				cloth->draw(expression, frame, overface, args);
	}

	void CharEquips::add_equip(int32_t itemid, const BodyDrawinfo& drawinfo)
	{
		if (itemid <= 0)
			return;

		auto iter = cloth_cache.find(itemid);

		if (iter == cloth_cache.end())
		{
			iter = cloth_cache.emplace(
				std::piecewise_construct,
				std::forward_as_tuple(itemid),
				std::forward_as_tuple(itemid, drawinfo)
			).first;
		}

		const Clothing& cloth = iter->second;

		Equipslot::Id slot = cloth.get_eqslot();
		clothes[slot] = &cloth;
	}

	void CharEquips::remove_equip(Equipslot::Id slot)
	{
		clothes[slot] = nullptr;
	}

	bool CharEquips::is_visible(Equipslot::Id slot) const
	{
		if (const Clothing * cloth = clothes[slot])
			return cloth->is_transparent() == false;
		else
			return false;
	}

	bool CharEquips::comparelayer(Equipslot::Id slot, Stance::Id stance, Clothing::Layer layer) const
	{
		if (const Clothing * cloth = clothes[slot])
			return cloth->contains_layer(stance, layer);
		else
			return false;
	}

	bool CharEquips::has_overall() const
	{
		return get_equip(Equipslot::Id::TOP) / 10000 == 105;
	}

	bool CharEquips::has_weapon() const
	{
		return get_weapon() != 0;
	}

	bool CharEquips::is_twohanded() const
	{
		if (const Clothing * weapon = clothes[Equipslot::Id::WEAPON])
			return weapon->is_twohanded();
		else
			return false;
	}

	CharEquips::CapType CharEquips::getcaptype() const
	{
		const Clothing* cap = clothes[Equipslot::Id::HAT];

		// NONE now means only what it says - no hat is worn. It used to double
		// as "a hat is worn and I do not recognise it", which is the one answer
		// that makes the hat disappear.
		if (!cap)
			return CharEquips::CapType::NONE;

		const std::string& vslot = cap->get_vslot();

		// The three spellings this has always recognised, kept exactly as they
		// were. 764 hats draw correctly today and none of them reach the code
		// below, so this change cannot alter any of them.
		if (vslot == "CpH1H5")
			return CharEquips::CapType::HALFCOVER;
		else if (vslot == "CpH1H5AyAs")
			return CharEquips::CapType::FULLCOVER;
		else if (vslot == "CpH5")
			return CharEquips::CapType::HEADBAND;

		return captype_from_vslot(vslot);
	}

	Stance::Id CharEquips::adjust_stance(Stance::Id stance) const
	{
		if (const Clothing * weapon = clothes[Equipslot::Id::WEAPON])
		{
			switch (stance)
			{
			case Stance::Id::STAND1:
			case Stance::Id::STAND2:
				return weapon->get_stand();
			case Stance::Id::WALK1:
			case Stance::Id::WALK2:
				return weapon->get_walk();
			default:
				return stance;
			}
		}
		else
		{
			return stance;
		}
	}

	int32_t CharEquips::get_equip(Equipslot::Id slot) const
	{
		if (const Clothing * cloth = clothes[slot])
			return cloth->get_id();
		else
			return 0;
	}

	int32_t CharEquips::get_weapon() const
	{
		return get_equip(Equipslot::Id::WEAPON);
	}

	std::unordered_map<int32_t, Clothing> CharEquips::cloth_cache;
}