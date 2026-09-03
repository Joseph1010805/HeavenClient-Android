//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "DueyHandlers.h"

#include "Helpers/ItemParser.h"

#include "../../Util/GiftBox.h"

namespace ms
{
	namespace
	{
		// WHAT THE SERVER SAID, IN WORDS.
		//
		// Duey answers with a code and nothing else - so every one of these
		// arrived as a number nobody saw, and a gift that could not be sent
		// was indistinguishable from one that worked. The codes are
		// DueyProcessor.Actions, read out of the server.
		const char* said(uint8_t operation)
		{
			switch (operation)
			{
			case 0x09: return nullptr;                 // enable actions: nothing to say
			case 0x0A: return "Not enough mesos - Duey charges 5,000 to carry it.";
			case 0x0B: return "Duey would not take that.";
			case 0x0C: return "There is nobody by that name.";
			case 0x0D: return "That character is on your own account.";
			case 0x0E: return "Their parcel counter is full.";
			case 0x0F: return "They cannot be sent anything just now.";
			case 0x10: return "They already have one of those and cannot hold two.";
			case 0x11: return "That is more mesos than Duey will carry.";
			case 0x12: return "Sent. They will find it at Duey's.";
			case 0x13: return "Something went wrong at the counter.";
			case 0x14: return nullptr;                 // enable actions
			case 0x15: return "Your bag is full - make room and try again.";
			case 0x16: return "You already have one of those and cannot hold two.";
			case 0x17: return "Collected.";
			default:   return nullptr;
			}
		}
	}

	void ParcelHandler::handle(InPacket& recv) const
	{
		uint8_t operation = recv.read_byte();

		// EVERYTHING WAITING, sent whole. Not a change to apply - the whole
		// counter, every time - so the list is replaced rather than merged.
		if (operation == 0x08)
		{
			recv.skip(1);

			uint8_t count = recv.read_byte();

			std::vector<GiftBox::Parcel> found;

			for (uint8_t i = 0; i < count; i++)
			{
				GiftBox::Parcel parcel;

				parcel.id = recv.read_int();

				// THIRTEEN BYTES WHATEVER THE NAME IS. A fixed field, not a
				// length-prefixed string - reading it as one would take the
				// first two letters for a length and lose the rest of the
				// packet, taking every parcel after it with them.
				parcel.from = recv.read_padded_string(13);

				parcel.mesos = recv.read_int();

				recv.skip(8);   // when it was sent

				// The note is 200 bytes ALWAYS, present or not.
				if (recv.read_int() == 1)
					parcel.note = recv.read_padded_string(200);
				else
					recv.skip(200);

				recv.skip(1);

				// A parcel may be mesos only, with no item in it at all.
				if (recv.read_byte() == 1)
				{
					// The same reader the trade table uses. An item on this
					// wire has a fiddly, version-specific shape and a second
					// copy of that knowledge here would be a second thing to
					// get wrong - and getting it wrong loses the rest of the
					// packet rather than one field.
					ItemParser::Skimmed skimmed = ItemParser::skim_item(recv);

					parcel.item = skimmed.id;
					parcel.count = skimmed.count;
				}

				found.push_back(parcel);
			}

			GiftBox::get().set_parcels(std::move(found));

			return;
		}

		if (const char* words = said(operation))
			GiftBox::get().set_result(words);
	}
}
