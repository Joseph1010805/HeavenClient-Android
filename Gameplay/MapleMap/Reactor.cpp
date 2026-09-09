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
#include "Reactor.h"

#include "../Util/Misc.h"

#include <nlnx/nx.hpp>

namespace ms
{
	Reactor::Reactor(int32_t o, int32_t r, int8_t s, Point<int16_t> p) : MapObject(o, p), rid(r), state(s)
	{
		std::string strid = string_format::extend_id(rid, 7);
		src = nl::nx::reactor[strid + ".img"];

		normal = src[0];
		animation_ended = true;
		dead = false;
		hittable = false;

		// ⚠ A REACTOR DOES NOT FALL. The map says where it is, exactly, and
		// that is where it stays for its whole life.
		//
		// It inherits a NORMAL physics body from MapObject, so update() was
		// handing it to gravity every frame. That went unnoticed for as long
		// as the spawn snapped each reactor down onto the foothold beneath it,
		// because gravity then had nothing left to do. Take the snap away -
		// which had to go, since it overrode the exact position the map gives
		// and dropped chests onto the wrong platform - and every reactor on
		// the map visibly fell out of the sky on arrival.
		//
		// FIXATED is the fix, and it is also the honest description: the
		// object was never meant to be simulated at all.
		phobj.type = PhysicsObject::Type::FIXATED;

		// A reactor is hittable if ANY of its states defines an `event` block -
		// the data's flag for "reacts to being hit". This used to look only at
		// the SPAWN state, so a reactor the server spawned in a state whose
		// node happens to carry no event read as un-hittable, and Combat then
		// skipped it entirely. That is why the Amherst boxes broke sometimes
		// and ignored you other times: it depended on the state they spawned
		// in. Scanning every state can only make more reactors hittable, never
		// fewer, and the server validates the real hit anyway.
		for (auto st : src)
		{
			bool is_number = !st.name().empty();

			for (char c : st.name())
				if (c < '0' || c > '9')
				{
					is_number = false;
					break;
				}

			if (!is_number)
				continue;	// info, and other non-state nodes

			for (auto sub : st)
				if (sub.name() == "event")
				{
					hittable = true;
					break;
				}

			if (hittable)
				break;
		}

		// Sounds live at Sound.img/Reactor.img/<id>/<state>/Hit - keyed by the
		// PLAIN reactor id, not the zero-padded name the reactor's own data
		// file uses, and split per state rather than into hit/break. The code
		// here previously asked for "hit" and "break" under the padded id;
		// none of those three names exist, so it silently found nothing.
		int32_t soundid = rid;

		// TEST ALIAS - reactor 2001, the Amherst boxes, has no sound entry of
		// its own; 2000 does, and Reactor.nx carries 0002000.img and
		// 0002001.img as two variants of the same crate, so they very likely
		// shared a noise. This is a guess made to prove the sound path works
		// at all. If some other reactor elsewhere is silent too, the fault is
		// in this code and the alias is hiding it - so treat a working box
		// here as evidence about 2001 only.
		if (soundid == 2001)
			soundid = 2000;

		nl::node sndsrc = nl::nx::sound["Reactor.img"][std::to_string(soundid)];

		for (auto state_node : sndsrc)
		{
			nl::node hit = state_node["Hit"];

			if (hit.data_type() == nl::node::type::audio)
				statesounds.emplace(
					static_cast<int8_t>(std::stoi(state_node.name())),
					Sound(hit)
				);
		}
	}

	void Reactor::draw(double viewx, double viewy, float alpha) const
	{
		Point<int16_t> absp = phobj.get_absolute(viewx, viewy, alpha);
		// ⚠ NO SHIFT. THE ORIGIN IS THE WHOLE ANSWER.
		//
		// This had a correction on it twice, and both were guesses about what
		// a reactor origin means. An audit of all 1135 reactor sprites in
		// Reactor.nx settles it:
		//
		//     "other" - neither centre nor bottom   634  (55%)
		//     centre  - origin.y == h/2             351  (30%)
		//     bottom  - origin.y == h               147  (12%)
		//     top     - origin.y == 0                 3
		//
		// and SEVENTY-SIX of them have an origin that cannot be a foot at all
		// - origin.y of 60 on a sprite 26 pixels tall, or -9. So origin.y is
		// not "how high the art sits above its feet"; it is simply the
		// sprite's anchor, and the map's y is where that anchor belongs.
		//
		// draw() already subtracts the origin. Anything added on top of that
		// is a second opinion about data that was never ambiguous - and every
		// version of it was right for one convention and wrong for the other
		// two, which is why the chests floated by exactly their own height.
		Point<int16_t> shift = Point<int16_t>(0, 0);

		if (animation_ended)
		{
			// TODO: Handle 'default' animations (horntail reactor floating)
			normal.draw(absp - shift, alpha);
		}
		else
		{
			// A state of 0, or one past the reactor's last frame, would make
			// .at() throw and take the client down with it. The server picks
			// the state, so this is not ours to guarantee.
			auto it = animations.find(state - 1);

			if (it != animations.end())
				it->second.draw(DrawArgument(absp - shift), 1.0);
			else
				normal.draw(absp - shift, alpha);
		}
	}

	int8_t Reactor::update(const Physics& physics)
	{
		physics.move_object(phobj);

		if (!animation_ended)
		{
			auto it = animations.find(state - 1);
			animation_ended = (it != animations.end()) ? it->second.update() : true;
		}

		if (animation_ended && dead)
			deactivate();

		return phobj.fhlayer;
	}

	void Reactor::set_state(int8_t state)
	{
		// The sound belongs to the state being LEFT - it is the noise that
		// state makes when struck, not the noise of arriving somewhere new.
		play_state_sound(this->state);

		if (hittable)
		{
			animations[this->state] = src[this->state]["hit"];
			animation_ended = false;
		}

		this->state = state;
	}

	void Reactor::destroy(int8_t, Point<int16_t>)
	{
		play_state_sound(this->state);
		animations[this->state] = src[this->state]["hit"];
		state++;
		dead = true;
		animation_ended = false;
	}

	void Reactor::revive(int8_t s, Point<int16_t> position)
	{
		// Back to how the constructor would have left it. The break animation
		// is cleared as well as the flags: a chest that reopens while still
		// holding its own smash frames would play them again the next time
		// anything touched it.
		state = s;
		dead = false;
		animation_ended = true;
		animations.clear();

		set_position(position.x(), position.y());

		makeactive();
	}

	void Reactor::play_state_sound(int8_t which)
	{
		auto it = statesounds.find(which);

		if (it != statesounds.end())
			it->second.play();
	}

	bool Reactor::is_hittable() const
	{
		return hittable;
	}

	bool Reactor::is_in_range(const Rectangle<int16_t>& range) const
	{
		if (!active)
			return false;

		Rectangle<int16_t> bounds(Point<int16_t>(-30, -normal.get_dimensions().y()), Point<int16_t>(normal.get_dimensions().x() - 10, 0)); //normal.get_bounds(); //animations.at(stance).get_bounds();
		bounds.shift(get_position());

		return range.overlaps(bounds);
	}
}