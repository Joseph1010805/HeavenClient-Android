//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "GiftBox.h"

namespace ms
{
	void GiftBox::set_parcels(std::vector<Parcel> found)
	{
		waiting = std::move(found);
	}

	void GiftBox::set_result(const std::string& said)
	{
		last_result = said;
	}
}
