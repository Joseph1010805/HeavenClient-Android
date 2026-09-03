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
#include "Stage.h"
#include "../Constants.h"
#include <cmath>
#include "../Graphics/GraphicsGL.h"
#include "QuestTracker.h"

#include "../Audio/Audio.h"
#include "../Character/SkillId.h"
#include "../IO/Messages.h"
#include "../IO/UI.h"
#include "../Util/Misc.h"

#include "../IO/UITypes/UIStatusbar.h"
#include "../Net/Packets/AttackAndSkillPackets.h"
#include "../Net/Packets/GameplayPackets.h"

#include <nlnx/nx.hpp>

namespace ms
{
	Stage::Stage() : combat(player, chars, mobs, reactors)
	{
		state = State::INACTIVE;
	}

	void Stage::init()
	{
		drops.init();
	}

	void Stage::load(int32_t mapid, int8_t portalid)
	{
		switch (state)
		{
		case State::INACTIVE:
			load_map(mapid);
			respawn(portalid);
			break;
		case State::TRANSITION:
			respawn(portalid);
			break;
		default:
			// ALREADY ACTIVE - AND THIS CASE DID NOT EXIST.
			//
			// A SET_FIELD arriving while the stage is active matched no case
			// at all, so the new map was never built: the tiles, footholds and
			// portals stayed those of the map before it, and every monster
			// standing on the old one stayed too. That is "it brought the
			// enemies over", and it is also why the cash shop trapped you -
			// you came back into geometry belonging to somewhere else, where
			// the portal you were standing on does not exist.
			//
			// Coming back to the SAME map is a respawn and must not rebuild
			// anything, or a portal within one map would flush the whole
			// place and re-download it.
			if (mapid != Stage::mapid)
			{
				// Not clear(): that dispatches a map-transfer packet and
				// tells the server something is happening. Nothing is - the
				// server already knows, it is the one that sent us here. Only
				// the objects of the old map go.
				chars.clear();
				npcs.clear();
				mobs.clear();
				drops.clear();
				reactors.clear();
				doors.clear();
				mists.clear();
				summons.clear();

				load_map(mapid);
			}

			respawn(portalid);
			break;
		}

		state = State::ACTIVE;
	}

	void Stage::loadplayer(const CharEntry& entry)
	{
		player = entry;
		playable = player;
	}

	void Stage::clear()
	{
		state = State::INACTIVE;

		chars.clear();
		npcs.clear();
		mobs.clear();
		drops.clear();
		reactors.clear();
		doors.clear();
		mists.clear();
		summons.clear();

		PlayerMapTransferPacket().dispatch();
	}

	void Stage::load_map(int32_t mapid)
	{
		Stage::mapid = mapid;
		std::string strid = string_format::extend_id(mapid, 9);
		std::string prefix = std::to_string(mapid / 100000000);
		nl::node src = nl::nx::map["Map"]["Map" + prefix][strid + ".img"];

		tilesobjs = MapTilesObjs(src);
		backgrounds = MapBackgrounds(src["back"]);
		physics = Physics(src["foothold"]);
		mapinfo = MapInfo(src, physics.get_fht().get_walls(), physics.get_fht().get_borders());
		portals = MapPortals(src["portal"], mapid);
	}

	void Stage::respawn(int8_t portalid)
	{
		Music(mapinfo.get_bgm()).play();

		Point<int16_t> spawnpoint = portals.get_portal_by_id(portalid);
		Point<int16_t> startpos = physics.get_y_below(spawnpoint);
		player.respawn(startpos, mapinfo.is_underwater());
		camera.set_position(startpos);
		camera.set_view(mapinfo.get_walls(), mapinfo.get_borders());
	}

	void Stage::draw_backdrop(double viewx, double viewy, float alpha) const
	{
		if (state != State::ACTIVE)
			return;

		// Only the BACKGROUND layers - no tiles, no objects, no mobs. The
		// panel wants the sky and the far scenery of wherever the party is
		// standing, not a second copy of the fight.
		backgrounds.drawbackgrounds(viewx, viewy, alpha);
	}

	Point<double> Stage::view_position(float alpha) const
	{
		return camera.realposition(alpha);
	}

	void Stage::draw(float alpha) const
	{
		if (state != State::ACTIVE)
			return;

		Point<int16_t> viewpos = camera.position(alpha);
		Point<double> viewrpos = camera.realposition(alpha);
		double viewx = viewrpos.x();
		double viewy = viewrpos.y();

		backgrounds.drawbackgrounds(viewx, viewy, alpha);

		for (auto id : Layer::IDs)
		{
			tilesobjs.draw(id, viewpos, alpha);
			reactors.draw(id, viewx, viewy, alpha);
			doors.draw(id, viewx, viewy, alpha);
			mists.draw(id, viewx, viewy, alpha);
			summons.draw(id, viewx, viewy, alpha);
			npcs.draw(id, viewx, viewy, alpha);
			mobs.draw(id, viewx, viewy, alpha);
			chars.draw(id, viewx, viewy, alpha);
			player.draw(id, viewx, viewy, alpha);
			drops.draw(id, viewx, viewy, alpha);
		}

		combat.draw(viewx, viewy, alpha);
		portals.draw(viewpos, alpha);
		backgrounds.drawforegrounds(viewx, viewy, alpha);
		effect.draw();

		// GO HERE.
		//
		// A marker bobbing over whoever the tracked quest wants, drawn after
		// the map so nothing in front of them hides it, and before the
		// foreground-independent UI so it still belongs to the world.
		//
		// The bob is not decoration: the NPCs on a Maple map stand in a row
		// of near-identical sprites, and a still marker reads as part of the
		// scenery. Movement is what the eye picks out.
		{
			Point<int16_t> target;
			bool found = QuestTracker::get().find_target(target);

			// AN ARROW OVER YOUR OWN HEAD, POINTING THE WAY.
			//
			// The marker over the NPC only helps once they are on screen, and
			// the whole problem is finding somebody who is not. This says
			// which way to walk from wherever you are standing.
			//
			// Only while they are OFF screen: once the chevron over their head
			// is visible, a second arrow telling you to look at it is noise.
			if (found)
			{
				Point<int16_t> me = player.get_position();

				int16_t dx = static_cast<int16_t>(target.x() - me.x());
				int16_t dy = static_cast<int16_t>(target.y() - me.y());

				int16_t adx = dx < 0 ? -dx : dx;
				int16_t ady = dy < 0 ? -dy : dy;

				int16_t half_w = static_cast<int16_t>(
					Constants::Constants::get().get_viewwidth() / 2);
				int16_t half_h = static_cast<int16_t>(
					Constants::Constants::get().get_viewheight() / 2);

				bool onscreen = adx < half_w - 40 && ady < half_h - 40;

				if (!onscreen)
				{
					static uint32_t swing = 0;
					double lean = std::sin(++swing * 0.06) * 3.0;

					Point<int16_t> at(
						static_cast<int16_t>(me.x() + viewpos.x()),
						static_cast<int16_t>(me.y() + viewpos.y() - 78 + lean));

					// FOUR WAYS, not an angle. This is a side-scroller: the
					// answer is nearly always "left" or "right", and a rotated
					// sprite at 22 degrees tells you less than a chevron that
					// unambiguously points one way. Up and down are kept for
					// the ladder and rope maps, where they are the answer.
					auto bar = [&](int16_t x, int16_t y, int16_t w, int16_t h)
					{
						GraphicsGL::get().drawrectangle(
							at.x() + x, at.y() + y, w, h,
							1.0f, 0.86f, 0.26f, 0.95f);
					};

					if (adx >= ady)
					{
						// Pointing sideways: a stack of bars stepping out to
						// the side you should walk.
						int16_t s = dx < 0 ? -1 : 1;

						bar(s < 0 ? -14 : 2, 6, 12, 4);
						bar(s < 0 ? -10 : 2, 2, 8, 4);
						bar(s < 0 ? -6 : 2, 10, 8, 4);
					}
					else if (dy < 0)
					{
						bar(-9, 8, 18, 4);
						bar(-5, 4, 10, 4);
						bar(-2, 0, 4, 4);
					}
					else
					{
						bar(-9, 0, 18, 4);
						bar(-5, 4, 10, 4);
						bar(-2, 8, 4, 4);
					}
				}
			}

			if (found)
			{
				// Counted in FRAMES rather than read off a clock - Stage has
				// no timer of its own and adding one for a bobbing arrow
				// would be a lot of machinery for five pixels.
				static uint32_t tick = 0;
				double bob = std::sin(++tick * 0.055) * 5.0;

				Point<int16_t> at(
					static_cast<int16_t>(target.x() + viewpos.x()),
					static_cast<int16_t>(target.y() + viewpos.y() - 74 + bob));

				// Drawn rather than taken from artwork: a downward chevron in
				// two bars, which is legible at any zoom and needs no node
				// that might not be in every version of the data.
				GraphicsGL::get().drawrectangle(at.x() - 9, at.y(), 18, 5,
					1.0f, 0.86f, 0.26f, 0.95f);
				GraphicsGL::get().drawrectangle(at.x() - 5, at.y() + 5, 10, 5,
					1.0f, 0.86f, 0.26f, 0.95f);
				GraphicsGL::get().drawrectangle(at.x() - 2, at.y() + 10, 4, 5,
					1.0f, 0.86f, 0.26f, 0.95f);
			}
		}
	}

	void Stage::update()
	{
		if (state != State::ACTIVE)
			return;

		combat.update();
		backgrounds.update();
		effect.update();
		tilesobjs.update();

		reactors.update(physics);
		doors.update(physics);
		mists.update(physics);
		summons.update(physics);
		npcs.update(physics);
		// Before the mobs move, not after - a mob deciding where to walk this
		// tick should be chasing where the player is now, not last tick.
		mobs.set_target(player.get_position());
		mobs.update(physics);
		chars.update(physics);
		drops.update(physics);
		player.update(physics);

		// Grabbing a rope was only ever tested at the instant up was pressed.
		// Jump up-and-right and the key goes down before the rope is reached,
		// so the one check happened too early and nothing looked again while
		// passing it - the grab was simply missed. Holding up now keeps
		// checking, which is what holding the key already means to a player.
		//
		// Safe to repeat: findladder only answers while actually overlapping a
		// rope, set_ladder(nullptr) does nothing, and check_ladders returns
		// early once climbing has started.
		if (player.is_key_down(KeyAction::Id::UP))
			check_ladders(true);

		portals.update(player.get_position());
		check_touch_portals();
		camera.update(player.get_position());

		if (player.is_invincible())
			return;

		if (int32_t oid_id = mobs.find_colliding(player.get_phobj()))
		{
			if (MobAttack attack = mobs.create_attack(oid_id))
			{
				MobAttackResult result = player.damage(attack);
				TakeDamagePacket(result, TakeDamagePacket::From::TOUCH).dispatch();
			}
		}

		// Swings that connected. `from` is the mob's attack index rather than
		// TOUCH(-1), which is how the server finds the same attack in its own
		// MobAttackInfo table - see TakeDamageHandler.
		for (auto& hit : mobs.take_landed_attacks(player.get_position()))
		{
			MobAttackResult result = player.damage(hit.second);

			TakeDamagePacket(hit.first, 0, result.damage,
				result.mobid, result.oid, result.direction).dispatch();
		}
	}

	void Stage::show_character_effect(int32_t cid, CharEffect::Id effect)
	{
		if (auto character = get_character(cid))
			character->show_effect_id(effect);
	}

	void Stage::check_touch_portals()
	{
		if (state != State::ACTIVE || player.is_attacking())
			return;

		Portal::WarpInfo touched = portals.find_touch_at(player.get_position());

		if (!touched.valid && !touched.scripted)
			return;

		// The server decides what a contact portal DOES - show a hint, play an
		// intro, warp, or refuse - so the client only reports having walked
		// into it. Same as a scripted portal entered with UP; the difference
		// is what set it off, not what happens next.
		ChangeMapPacket(false, -1, touched.name, false).dispatch();
	}

	void Stage::check_portals()
	{
		if (player.is_attacking())
			return;

		Point<int16_t> playerpos = player.get_position();
		Portal::WarpInfo warpinfo = portals.find_warp_at(playerpos);

		if (warpinfo.intramap)
		{
			Point<int16_t> spawnpoint = portals.get_portal_by_name(warpinfo.toname);
			Point<int16_t> startpos = physics.get_y_below(spawnpoint);

			player.respawn(startpos, mapinfo.is_underwater());
		}
		else if (warpinfo.scripted)
		{
			// Say which portal was entered and let the server answer.
			//
			// Where a scripted portal leads is not written in the map - the
			// server works it out from the character's quests and sends a warp
			// back, or refuses with a message ("Please click on the NPC first
			// to receive a quest"). So the map id must NOT be set here: there
			// isn't one yet, and claiming 999999999 would leave the client
			// believing it had moved somewhere that does not exist.
			ChangeMapPacket(false, -1, warpinfo.name, false).dispatch();
		}
		else if (warpinfo.valid)
		{
			ChangeMapPacket(false, -1, warpinfo.name, false).dispatch();

			CharStats& stats = Stage::get().get_player().get_stats();

			stats.set_mapid(warpinfo.mapid);

			Sound(Sound::Name::PORTAL).play();
		}
	}

	void Stage::check_seats()
	{
		if (player.is_sitting() || player.is_attacking())
			return;

		Optional<const Seat> seat = mapinfo.findseat(player.get_position());
		player.set_seat(seat);
	}

	void Stage::check_ladders(bool up)
	{
		if (player.is_climbing() || player.is_attacking())
			return;

		Optional<const Ladder> ladder = mapinfo.findladder(player.get_position(), up);
		player.set_ladder(ladder);
	}

	void Stage::check_drops()
	{
		Point<int16_t> playerpos = player.get_position();
		MapDrops::Loot loot = drops.find_loot_at(playerpos);

		if (loot.first)
			PickupItemPacket(loot.first, loot.second).dispatch();
	}

	bool Stage::is_active() const
	{
		return state == State::ACTIVE;
	}

	void Stage::send_key(KeyType::Id type, int32_t action, bool down)
	{
		if (state != State::ACTIVE || !playable)
			return;

		switch (type)
		{
		case KeyType::Id::ACTION:
			if (down)
			{
				switch (action)
				{
				case KeyAction::Id::UP:
					check_ladders(true);
					check_portals();
					break;
				case KeyAction::Id::DOWN:
					check_ladders(false);
					break;
				case KeyAction::Id::SIT:
					check_seats();
					break;
				case KeyAction::Id::ATTACK:
					combat.use_move(0);
					break;
				case KeyAction::Id::PICKUP:
					check_drops();
					break;
				}
			}

			playable->send_action(KeyAction::actionbyid(action), down);
			break;
		case KeyType::Id::SKILL:
			combat.use_move(action);
			break;
		case KeyType::Id::ITEM:
		case KeyType::Id::CASH:
			// CASH FELL THROUGH THIS SWITCH ENTIRELY.
			//
			// UIItemInventory tags a hotkey from the cash tab as CASH so the
			// quickslot bar and the server can tell the two inventories
			// apart - and then nothing here handled it, so a pet, a chair or
			// a megaphone on a key did nothing and said nothing.
			//
			// use_item looks the item up by its own id and picks the right
			// packet, so both kinds arrive in the same place.
			player.use_item(action);
			break;
		case KeyType::Id::FACE:
			player.set_expression(action);
			break;
		}
	}

	Cursor::State Stage::send_cursor(bool pressed, Point<int16_t> position)
	{
		auto statusbar = UI::get().get_element<UIStatusbar>();

		if (statusbar && statusbar->is_menu_active())
		{
			if (pressed)
			{
				statusbar->remove_menus();

				return npcs.send_cursor(pressed, position, camera.position());
			}

			return statusbar->send_cursor(pressed, position);
		}

		return npcs.send_cursor(pressed, position, camera.position());
	}

	bool Stage::is_player(int32_t cid) const
	{
		return cid == player.get_oid();
	}

	MapNpcs& Stage::get_npcs()
	{
		return npcs;
	}

	MapChars& Stage::get_chars()
	{
		return chars;
	}

	MapMobs& Stage::get_mobs()
	{
		return mobs;
	}

	MapReactors& Stage::get_reactors()
	{
		return reactors;
	}

	MapDoors& Stage::get_doors()
	{
		return doors;
	}

	MapMists& Stage::get_mists()
	{
		return mists;
	}

	MapSummons& Stage::get_summons()
	{
		return summons;
	}

	MapDrops& Stage::get_drops()
	{
		return drops;
	}

	Player& Stage::get_player()
	{
		return player;
	}

	Combat& Stage::get_combat()
	{
		return combat;
	}

	Optional<Char> Stage::get_character(int32_t cid)
	{
		if (is_player(cid))
			return player;
		else
			return chars.get_char(cid);
	}

	int Stage::get_mapid()
	{
		return mapid;
	}

	void Stage::add_effect(std::string path)
	{
		effect = MapEffect(path);
	}
}