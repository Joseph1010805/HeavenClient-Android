//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ms
{
	// WHAT IS WAITING AT DUEY'S COUNTER.
	//
	// The parcels the server last told us about, and the last thing it said
	// about a send. Nothing here is saved to disk and nothing here is a
	// SOURCE of truth: the server owns the counter, this is only the most
	// recent answer it gave, refreshed by asking again.
	//
	// Deliberately unlike PostBox, which queues on the device because it has
	// to work with no network. A gift cannot be queued that way - the item
	// has to leave your bag on the server for the parcel to exist at all, so
	// there is nothing to do offline but wait.
	class GiftBox
	{
	public:
		static GiftBox& get()
		{
			static GiftBox instance;

			return instance;
		}

		struct Parcel
		{
			int32_t id = 0;
			std::string from;
			int32_t mesos = 0;
			std::string note;

			// What is in it, if anything. A parcel may be mesos only.
			int32_t item = 0;
			int16_t count = 0;
		};

		const std::vector<Parcel>& parcels() const { return waiting; }

		// Replace the list wholesale. The server sends all of it every time,
		// so merging would only be a way to keep a parcel that has already
		// been collected.
		void set_parcels(std::vector<Parcel> found);

		// WHAT THE SERVER SAID ABOUT THE LAST ATTEMPT, in words a screen can
		// show - "There is no player by that name", and so on.
		//
		// Duey answers a failed send with a CODE and nothing else, so without
		// this a gift that could not be sent looked exactly like one that
		// worked: the page closed and nothing happened.
		void set_result(const std::string& said);

		const std::string& result() const { return last_result; }

		void clear_result() { last_result.clear(); }

		// Whether anything is waiting, for the badge on the way in.
		bool anything() const { return !waiting.empty(); }

	private:
		GiftBox() = default;

		std::vector<Parcel> waiting;
		std::string last_result;
	};
}
